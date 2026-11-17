/**
 * Echenon Diagnostics Tab — Event Log Viewer + Runaway Warning Banner
 *
 * Self-registering tab module for the ESP32Modular portal framework.
 * Listens for 'tab-changed' events with detail === 'tab-diagnostics'.
 *
 * Endpoints consumed:
 *   GET /api/echenon/events?limit=50
 *   GET /api/echenon/live (or LiveFeed WS for runaway state)
 */
(function() {
  'use strict';

  var TAB_ID = 'tab-diagnostics';
  var POLL_MS = 30000;  // Auto-refresh every 30 seconds
  var RUNAWAY_POLL_MS = 5000;  // Poll runaway status every 5 seconds

  var initialized = false;
  var isTabActive = false;
  var pollTimer = null;
  var runawayTimer = null;
  var stylesInjected = false;

  // --- Event Type Badge Classes ---
  var TYPE_BADGES = {
    'BOOT':             { cls: 'diag-badge--boot',        label: 'Boot' },
    'SESSION_START':    { cls: 'diag-badge--start',       label: 'Session Start' },
    'SESSION_END':      { cls: 'diag-badge--end',         label: 'Session End' },
    'RUNAWAY_ALERT':    { cls: 'diag-badge--runaway',     label: 'Runaway' },
    'CALIBRATION_DONE': { cls: 'diag-badge--calibration', label: 'Calibration' },
    'FIRMWARE_CHANGE':  { cls: 'diag-badge--firmware',    label: 'Firmware' }
  };

  function injectStyles() {
    if (stylesInjected) return;
    stylesInjected = true;
    var style = document.createElement('style');
    style.textContent = [
      '.diag-card {',
      '  background: var(--card-bg);',
      '  border-radius: 0.75rem;',
      '  padding: 1rem;',
      '  margin-bottom: 1rem;',
      '}',
      '.diag-runaway-banner {',
      '  background: var(--danger-bg, rgba(198, 40, 40, 0.08));',
      '  border: 1px solid var(--error, #c62828);',
      '  border-radius: 0.75rem;',
      '  padding: 1rem;',
      '  margin-bottom: 1rem;',
      '  display: none;',
      '}',
      '.diag-runaway-banner.active {',
      '  display: block;',
      '}',
      '.diag-runaway-title {',
      '  color: var(--error, #c62828);',
      '  font-weight: 700;',
      '  font-size: 0.95rem;',
      '  margin-bottom: 0.25rem;',
      '}',
      '[data-theme="dark"] .diag-runaway-title {',
      '  color: var(--danger-light, #f87171);',
      '}',
      '.diag-runaway-text {',
      '  color: var(--text);',
      '  font-size: 0.82rem;',
      '}',
      '.diag-table-wrap {',
      '  max-height: 400px;',
      '  overflow-y: auto;',
      '  border-radius: 0.5rem;',
      '}',
      '.diag-table {',
      '  width: 100%;',
      '  border-collapse: collapse;',
      '  font-size: 0.82rem;',
      '}',
      '.diag-table th {',
      '  position: sticky;',
      '  top: 0;',
      '  background: var(--card-bg);',
      '  text-align: left;',
      '  padding: 0.5rem 0.75rem;',
      '  color: var(--text-muted);',
      '  font-weight: 600;',
      '  text-transform: uppercase;',
      '  letter-spacing: 0.05em;',
      '  font-size: 0.7rem;',
      '  border-bottom: 1px solid var(--border);',
      '}',
      '.diag-table td {',
      '  padding: 0.5rem 0.75rem;',
      '  border-bottom: 1px solid var(--border);',
      '}',
      '.diag-badge {',
      '  display: inline-block;',
      '  padding: 0.15rem 0.5rem;',
      '  border-radius: 9999px;',
      '  font-size: 0.7rem;',
      '  font-weight: 600;',
      '  letter-spacing: 0.03em;',
      '  background: var(--border, #e0e0e0);',
      '  color: var(--text, #212529);',
      '}',
      '.diag-badge--boot { background: var(--primary-hover, #1976D2); color: var(--text-inverse, #ffffff); }',
      '.diag-badge--start { background: var(--success, #2e7d32); color: var(--text-inverse, #ffffff); }',
      '.diag-badge--end { background: var(--indigo, #4338ca); color: var(--text-inverse, #ffffff); }',
      '.diag-badge--runaway { background: var(--error, #c62828); color: var(--text-inverse, #ffffff); }',
      '.diag-badge--calibration { background: var(--warning, #f9a825); color: var(--text-on-warning, #111827); }',
      '.diag-badge--firmware { background: var(--purple, #6d28d9); color: var(--text-inverse, #ffffff); }',
      '.diag-empty {',
      '  text-align: center;',
      '  color: var(--text-muted);',
      '  padding: 2rem;',
      '  font-size: 0.85rem;',
      '}',
      '.diag-header {',
      '  display: flex;',
      '  justify-content: space-between;',
      '  align-items: center;',
      '  margin-bottom: 0.75rem;',
      '}',
      '.diag-header h3 {',
      '  margin: 0;',
      '  font-size: 0.95rem;',
      '}',
      '.diag-refresh-btn {',
      '  background: none;',
      '  border: 1px solid var(--border);',
      '  border-radius: 0.375rem;',
      '  padding: 0.25rem 0.65rem;',
      '  font-size: 0.75rem;',
      '  color: var(--text-muted);',
      '  cursor: pointer;',
      '}',
      '.diag-refresh-btn:hover {',
      '  border-color: var(--primary);',
      '  color: var(--primary);',
      '}'
    ].join('\n');
    document.head.appendChild(style);
  }

  function formatTime(epochS) {
    if (!epochS || epochS === 0) return '—';
    try {
      var d = new Date(epochS * 1000);
      return d.toLocaleString(undefined, {
        month: 'short', day: 'numeric',
        hour: '2-digit', minute: '2-digit', second: '2-digit'
      });
    } catch(e) {
      return String(epochS);
    }
  }

  function makeBadge(type) {
    var info = TYPE_BADGES[type] || { cls: '', label: type };
    var cls = info.cls ? ('diag-badge ' + info.cls) : 'diag-badge';
    return '<span class="' + cls + '">' + info.label + '</span>';
  }

  function formatDetails(ev) {
    var d = ev.payload || ev.details;
    if (!d) return '—';
    var parts = [];
    if (d.uptime_s !== undefined) parts.push('Uptime: ' + d.uptime_s + 's');
    if (d.session_id !== undefined) parts.push('Session: ' + d.session_id);
    if (d.duration_s !== undefined) parts.push('Duration: ' + d.duration_s + 's');
    if (d.duration_ms !== undefined) parts.push('Duration: ' + d.duration_ms + 'ms');
    var rec = (d.record_count_approx !== undefined) ? d.record_count_approx : d.record_count;
    if (rec !== undefined) parts.push('Records: ' + rec);
    if (d.calibration_type !== undefined) {
      var cal = d.calibration_type;
      var calLabel = (cal === 'initial' || cal === 0) ? 'Initial'
                   : (cal === 'recalibration' || cal === 1) ? 'Recalibration'
                   : String(cal);
      parts.push('Type: ' + calLabel);
    }
    if (d.version !== undefined) parts.push('Version: ' + d.version);
    return parts.length ? parts.join(', ') : '—';
  }

  function renderEvents(events) {
    var tbody = document.getElementById('diag-events-tbody');
    var empty = document.getElementById('diag-events-empty');
    if (!tbody) return;

    if (!events || events.length === 0) {
      tbody.innerHTML = '';
      if (empty) empty.style.display = 'block';
      return;
    }
    if (empty) empty.style.display = 'none';

    var html = '';
    for (var i = 0; i < events.length; i++) {
      var ev = events[i];
      html += '<tr>' +
        '<td>' + formatTime(ev.epoch || ev.epoch_s) + '</td>' +
        '<td>' + makeBadge(ev.type) + '</td>' +
        '<td>' + formatDetails(ev) + '</td>' +
        '</tr>';
    }
    tbody.innerHTML = html;
  }

  function apiJson(url, options) {
    var opts = Object.assign({ credentials: 'same-origin' }, options || {});
    if (typeof window.apiFetch === 'function') {
      return window.apiFetch(url, opts);
    }
    return fetch(url, opts).then(function(r) {
      if (!r.ok) throw new Error('HTTP ' + r.status);
      return r.json();
    });
  }

  function fetchEvents() {
    apiJson('/api/echenon/events?limit=50')
      .then(function(data) {
        renderEvents(data.events || data);
      })
      .catch(function() {});
  }

  function checkRunaway() {
    var banner = document.getElementById('diag-runaway-banner');
    if (!banner) return;

    apiJson('/api/echenon/live')
      .then(function(data) {
        if (data && data.runaway) {
          banner.classList.add('active');
        } else {
          banner.classList.remove('active');
        }
      })
      .catch(function() {
        // If endpoint unavailable, hide banner
        banner.classList.remove('active');
      });
  }

  function startPolling() {
    stopPolling();
    fetchEvents();
    checkRunaway();
    pollTimer = setInterval(fetchEvents, POLL_MS);
    runawayTimer = setInterval(checkRunaway, RUNAWAY_POLL_MS);
  }

  function stopPolling() {
    if (pollTimer) { clearInterval(pollTimer); pollTimer = null; }
    if (runawayTimer) { clearInterval(runawayTimer); runawayTimer = null; }
  }

  function initTab(container) {
    injectStyles();

    container.innerHTML = [
      '<div id="diag-runaway-banner" class="diag-runaway-banner" role="alert">',
      '  <div class="diag-runaway-title">\u26A0\uFE0F Resistance Runaway Detected</div>',
      '  <div class="diag-runaway-text">The resistance sensor is reading outside the calibrated range for an extended period.</div>',
      '</div>',
      '<div class="diag-card">',
      '  <div class="diag-header">',
      '    <h3>Event Log</h3>',
      '    <button id="diag-refresh-btn" class="diag-refresh-btn">Refresh</button>',
      '  </div>',
      '  <div class="diag-table-wrap">',
      '    <table class="diag-table">',
      '      <thead><tr><th>Time</th><th>Type</th><th>Details</th></tr></thead>',
      '      <tbody id="diag-events-tbody"></tbody>',
      '    </table>',
      '  </div>',
      '  <div id="diag-events-empty" class="diag-empty">No events recorded yet.</div>',
      '</div>'
    ].join('\n');

    var refreshBtn = document.getElementById('diag-refresh-btn');
    if (refreshBtn) {
      refreshBtn.addEventListener('click', function() {
        fetchEvents();
        checkRunaway();
      });
    }
  }

  window.addEventListener('tab-changed', function(e) {
    var detail = e.detail;
    // Support both string detail and object detail.tab
    var tabId = (typeof detail === 'string') ? detail : (detail && detail.tab ? detail.tab : '');

    if (tabId === TAB_ID) {
      isTabActive = true;
      var container = document.getElementById(TAB_ID);
      if (!container) return;

      if (!initialized) {
        initialized = true;
        initTab(container);
      }
      startPolling();
    } else {
      if (isTabActive) {
        isTabActive = false;
        stopPolling();
      }
    }
  });

})();

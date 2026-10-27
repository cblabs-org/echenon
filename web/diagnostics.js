/**
 * Echenon Diagnostics Tab — Event Log Viewer + Runaway Warning Banner
 *
 * Self-registering tab module for the ESP32Modular portal framework.
 * Listens for 'tab-changed' events with detail === 'tab-diagnostics'.
 *
 * Endpoints consumed:
 *   GET /api/echenon/events?limit=50
 *   GET /api/echenon/status (or LiveFeed WS for runaway state)
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

  // --- Event Type Badge Colours ---
  var TYPE_COLOURS = {
    'BOOT':             { bg: '#2563eb22', fg: '#3b82f6', label: 'Boot' },
    'SESSION_START':    { bg: '#10b98122', fg: '#10b981', label: 'Session Start' },
    'SESSION_END':      { bg: '#6366f122', fg: '#818cf8', label: 'Session End' },
    'RUNAWAY_ALERT':    { bg: '#ef444422', fg: '#ef4444', label: 'Runaway' },
    'CALIBRATION_DONE': { bg: '#f59e0b22', fg: '#f59e0b', label: 'Calibration' },
    'FIRMWARE_CHANGE':  { bg: '#8b5cf622', fg: '#a78bfa', label: 'Firmware' }
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
      '  background: #dc262622;',
      '  border: 1px solid #ef4444;',
      '  border-radius: 0.75rem;',
      '  padding: 1rem;',
      '  margin-bottom: 1rem;',
      '  display: none;',
      '}',
      '.diag-runaway-banner.active {',
      '  display: block;',
      '}',
      '.diag-runaway-title {',
      '  color: #ef4444;',
      '  font-weight: 700;',
      '  font-size: 0.95rem;',
      '  margin-bottom: 0.25rem;',
      '}',
      '.diag-runaway-text {',
      '  color: var(--text-muted);',
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
      '}',
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
    var info = TYPE_COLOURS[type] || { bg: '#88888822', fg: '#888888', label: type };
    return '<span class="diag-badge" style="background:' + info.bg +
           ';color:' + info.fg + '">' + info.label + '</span>';
  }

  function formatDetails(ev) {
    if (!ev.details) return '—';
    var d = ev.details;
    var parts = [];
    if (d.uptime_s !== undefined) parts.push('Uptime: ' + d.uptime_s + 's');
    if (d.session_id !== undefined) parts.push('Session: ' + d.session_id);
    if (d.duration_s !== undefined) parts.push('Duration: ' + d.duration_s + 's');
    if (d.duration_ms !== undefined) parts.push('Duration: ' + d.duration_ms + 'ms');
    if (d.record_count !== undefined) parts.push('Records: ' + d.record_count);
    if (d.calibration_type !== undefined) {
      parts.push('Type: ' + (d.calibration_type === 0 ? 'Initial' : 'Recalibration'));
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
        '<td>' + formatTime(ev.epoch_s) + '</td>' +
        '<td>' + makeBadge(ev.type) + '</td>' +
        '<td>' + formatDetails(ev) + '</td>' +
        '</tr>';
    }
    tbody.innerHTML = html;
  }

  function fetchEvents() {
    if (typeof window.apiFetch === 'function') {
      window.apiFetch('/api/echenon/events?limit=50')
        .then(function(r) { return r.json(); })
        .then(function(data) {
          renderEvents(data.events || data);
        })
        .catch(function() {});
    } else {
      fetch('/api/echenon/events?limit=50')
        .then(function(r) { return r.json(); })
        .then(function(data) {
          renderEvents(data.events || data);
        })
        .catch(function() {});
    }
  }

  function checkRunaway() {
    var banner = document.getElementById('diag-runaway-banner');
    if (!banner) return;

    var fetchFn = (typeof window.apiFetch === 'function') ? window.apiFetch : fetch;
    fetchFn('/api/echenon/status')
      .then(function(r) { return r.json(); })
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
      '<div id="diag-runaway-banner" class="diag-runaway-banner">',
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

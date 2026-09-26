// web/echenon-dashboard.js
// Echenon Signal Logger Live Dashboard
// Displays live capture telemetry, decimated ADC channels, burst controls, and CSV export links.

(function() {
  'use strict';

  var CHANNEL_MAP = [
    { ch: 0, name: 'Pot Wiper', color: 'Blue' },
    { ch: 1, name: 'Pot HI', color: 'Yellow' },
    { ch: 2, name: 'Pot LO', color: 'Green' },
    { ch: 3, name: 'Cadence Return', color: 'Grey' },
    { ch: 4, name: 'Cadence', color: 'Orange' },
    { ch: 5, name: 'Motor+', color: 'Red' },
    { ch: 6, name: 'Motor−', color: 'Black' },
    { ch: 7, name: 'Encoder A', color: 'Brown' },
    { ch: 8, name: 'Encoder B', color: 'Orange' },
    { ch: 9, name: 'Encoder VCC', color: 'Red' }
  ];

  var isInitialized = false;
  var isTabActive = false;
  var pollTimer = null;
  var isBurstActive = false;

  function formatUptime(ms) {
    if (!ms && ms !== 0) return '-';
    var totalSec = Math.floor(ms / 1000);
    var hours = Math.floor(totalSec / 3600);
    var mins = Math.floor((totalSec % 3600) / 60);
    var secs = totalSec % 60;
    if (hours > 0) {
      return hours + 'h ' + mins + 'm ' + secs + 's';
    }
    return mins + 'm ' + secs + 's';
  }

  function formatBytes(bytes) {
    if (!bytes && bytes !== 0) return '-';
    if (bytes >= 1048576) {
      return (bytes / 1048576).toFixed(2) + ' MB';
    }
    if (bytes >= 1024) {
      return (bytes / 1024).toFixed(1) + ' KB';
    }
    return bytes + ' B';
  }

  function formatRate(rate) {
    if (!rate && rate !== 0) return '-';
    if (rate >= 1000) {
      return (rate / 1000).toFixed(1) + ' kSa/s';
    }
    return rate + ' Sa/s';
  }

  function renderScaffold(container) {
    var channelRows = CHANNEL_MAP.map(function(ch) {
      return (
        '<tr id="echenon-row-ch' + ch.ch + '">' +
          '<td style="font-weight:700; color:var(--text);">' + ch.ch + '</td>' +
          '<td>' + ch.name + ' <small class="text-muted">(' + ch.color + ')</small></td>' +
          '<td id="echenon-ch' + ch.ch + '-min" style="text-align:right; font-family:monospace;">-</td>' +
          '<td id="echenon-ch' + ch.ch + '-mean" style="text-align:right; font-family:monospace; font-weight:700; color:var(--primary);">-</td>' +
          '<td id="echenon-ch' + ch.ch + '-max" style="text-align:right; font-family:monospace;">-</td>' +
        '</tr>'
      );
    }).join('');

    container.innerHTML =
      '<div class="card" style="margin-bottom:1rem;">' +
        '<div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:0.75rem;">' +
          '<h3 style="margin:0; font-size:1.15rem; font-weight:700;">Capture Status</h3>' +
          '<div style="display:flex; align-items:center; gap:0.5rem;">' +
            '<span class="dot dot--green" id="echenon-status-dot"></span>' +
            '<span id="echenon-capture-state" style="font-size:0.85rem; font-weight:700; text-transform:uppercase;">Running</span>' +
          '</div>' +
        '</div>' +
        '<div class="info-row">' +
          '<span class="text-muted">Burst Status</span>' +
          '<span id="echenon-burst-badge" class="badge">IDLE</span>' +
        '</div>' +
        '<div class="info-row">' +
          '<span class="text-muted">Uptime</span>' +
          '<span id="echenon-uptime" style="font-family:monospace;">-</span>' +
        '</div>' +
        '<div class="info-row">' +
          '<span class="text-muted">Ride Log Rows</span>' +
          '<span id="echenon-rows" style="font-family:monospace;">-</span>' +
        '</div>' +
        '<div class="info-row">' +
          '<span class="text-muted">Sample Rate</span>' +
          '<span id="echenon-rate" style="font-family:monospace;">-</span>' +
        '</div>' +
        '<div class="info-row">' +
          '<span class="text-muted">Free PSRAM</span>' +
          '<span id="echenon-psram" style="font-family:monospace;">-</span>' +
        '</div>' +
        '<div class="info-row">' +
          '<span class="text-muted">Edge Events</span>' +
          '<span id="echenon-events-count" style="font-family:monospace;">-</span>' +
        '</div>' +
      '</div>' +

      '<div class="card" style="margin-bottom:1rem;">' +
        '<h3 style="margin:0 0 0.5rem 0; font-size:1.15rem; font-weight:700;">Trigger Burst Capture</h3>' +
        '<p style="font-size:0.85rem; color:var(--text-muted); margin:0 0 1rem 0;">' +
          'High-speed raw ADC window (83.3 kSa/s total across all 10 channels) into PSRAM.' +
        '</p>' +
        '<div class="compact-row" style="margin-bottom:0.5rem;">' +
          '<label style="display:flex; flex-direction:column; gap:0.25rem;">' +
            '<span style="font-size:0.85rem; font-weight:600;">Duration (ms)</span>' +
            '<input type="number" id="echenon-burst-duration" value="10000" min="1000" max="30000" step="1000" style="padding:0.6rem; border-radius:8px; border:1px solid var(--border); background:var(--bg); color:var(--text);">' +
          '</label>' +
          '<button type="button" id="echenon-burst-btn" style="height:46px; align-self:flex-end;">Start Burst</button>' +
        '</div>' +
      '</div>' +

      '<div class="card" style="margin-bottom:1rem;">' +
        '<h3 style="margin:0 0 0.5rem 0; font-size:1.15rem; font-weight:700;">Data Export (CSV)</h3>' +
        '<p style="font-size:0.85rem; color:var(--text-muted); margin:0 0 1rem 0;">' +
          'Download logged sensor data directly from device memory.' +
        '</p>' +
        '<div style="display:flex; flex-direction:column; gap:0.6rem;">' +
          '<a href="/api/echenon/ride-log.csv" download="ride-log.csv" id="echenon-dl-ride" class="button secondary" style="display:flex; align-items:center; justify-content:center; text-decoration:none; padding:0.75rem 1rem; border-radius:10px; font-weight:600; font-size:0.95rem; border:1.5px solid var(--border); color:var(--text);">' +
            'Download Ride Log CSV' +
          '</a>' +
          '<a href="/api/echenon/events.csv" download="events.csv" id="echenon-dl-events" class="button secondary" style="display:flex; align-items:center; justify-content:center; text-decoration:none; padding:0.75rem 1rem; border-radius:10px; font-weight:600; font-size:0.95rem; border:1.5px solid var(--border); color:var(--text);">' +
            'Download Events CSV' +
          '</a>' +
          '<a href="/api/echenon/burst.csv" download="burst.csv" id="echenon-dl-burst" class="button secondary" style="display:flex; align-items:center; justify-content:center; text-decoration:none; padding:0.75rem 1rem; border-radius:10px; font-weight:600; font-size:0.95rem; border:1.5px solid var(--border); color:var(--text);">' +
            'Download Burst CSV' +
          '</a>' +
        '</div>' +
      '</div>' +

      '<div class="card" style="margin-bottom:1rem;">' +
        '<div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:0.75rem;">' +
          '<h3 style="margin:0; font-size:1.15rem; font-weight:700;">Live Channels (Decimated)</h3>' +
          '<small class="text-muted" id="echenon-channels-timestamp">20 Hz decimated</small>' +
        '</div>' +
        '<div style="overflow-x:auto;">' +
          '<table style="width:100%; border-collapse:collapse; font-size:0.85rem;">' +
            '<thead>' +
              '<tr style="border-bottom:2px solid var(--border); color:var(--text-muted); text-align:left;">' +
                '<th style="padding:6px 4px; width:10%;">Ch</th>' +
                '<th style="padding:6px 4px; width:45%;">Signal</th>' +
                '<th style="padding:6px 4px; width:15%; text-align:right;">Min</th>' +
                '<th style="padding:6px 4px; width:15%; text-align:right;">Mean</th>' +
                '<th style="padding:6px 4px; width:15%; text-align:right;">Max</th>' +
              '</tr>' +
            '</thead>' +
            '<tbody>' +
              channelRows +
            '</tbody>' +
          '</table>' +
        '</div>' +
      '</div>';

    // Wire burst button
    var burstBtn = document.getElementById('echenon-burst-btn');
    var durationInput = document.getElementById('echenon-burst-duration');
    if (burstBtn && durationInput) {
      burstBtn.addEventListener('click', function() {
        var duration = parseInt(durationInput.value, 10) || 10000;
        if (duration < 1000) duration = 1000;
        if (duration > 30000) duration = 30000;
        triggerBurst(duration);
      });
    }
  }

  async function triggerBurst(durationMs) {
    var burstBtn = document.getElementById('echenon-burst-btn');
    if (burstBtn) burstBtn.setAttribute('aria-busy', 'true');

    try {
      var fetchFn = window.apiFetch || window.fetch;
      var resp = await fetchFn('/api/echenon/burst/start', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ duration_ms: durationMs })
      });

      if (resp.status === 409) {
        if (window.showToast) window.showToast('Burst capture already active', 'warning');
      } else if (resp.ok) {
        if (window.showToast) window.showToast('Burst capture started (' + durationMs + ' ms)', 'success');
        updateBurstUI(true);
      } else {
        var errJson = await resp.json().catch(function() { return {}; });
        var msg = errJson.error || ('Burst trigger failed (' + resp.status + ')');
        if (window.showToast) window.showToast(msg, 'error');
      }
    } catch (err) {
      if (window.showToast) window.showToast('Network error: ' + err.message, 'error');
    } finally {
      if (burstBtn) burstBtn.removeAttribute('aria-busy');
    }
  }

  function updateBurstUI(active) {
    isBurstActive = !!active;
    var badge = document.getElementById('echenon-burst-badge');
    var btn = document.getElementById('echenon-burst-btn');
    if (badge) {
      if (isBurstActive) {
        badge.textContent = 'RECORDING';
        badge.style.background = 'var(--error)';
        badge.style.color = '#fff';
      } else {
        badge.textContent = 'IDLE';
        badge.style.background = '';
        badge.style.color = '';
      }
    }
    if (btn) {
      btn.disabled = isBurstActive;
      btn.textContent = isBurstActive ? 'Burst in Progress...' : 'Start Burst';
    }
  }

  async function fetchStatus() {
    var fetchFn = window.apiFetch || window.fetch;
    try {
      var resp = await fetchFn('/api/echenon/status');
      if (!resp.ok) throw new Error('HTTP ' + resp.status);
      var data = await resp.json();

      var uptimeEl = document.getElementById('echenon-uptime');
      var rowsEl = document.getElementById('echenon-rows');
      var rateEl = document.getElementById('echenon-rate');
      var psramEl = document.getElementById('echenon-psram');
      var dotEl = document.getElementById('echenon-status-dot');
      var stateEl = document.getElementById('echenon-capture-state');

      if (uptimeEl) uptimeEl.textContent = formatUptime(data.uptime_ms);
      if (rowsEl) rowsEl.textContent = Number(data.ride_log_rows).toLocaleString();
      if (rateEl) rateEl.textContent = formatRate(data.sample_rate);
      if (psramEl) psramEl.textContent = formatBytes(data.free_psram);

      if (dotEl) {
        dotEl.className = 'dot dot--green';
      }
      if (stateEl) {
        stateEl.textContent = 'Running';
        stateEl.style.color = 'var(--success)';
      }

      updateBurstUI(data.burst_active);
    } catch (err) {
      var dot = document.getElementById('echenon-status-dot');
      var state = document.getElementById('echenon-capture-state');
      if (dot) dot.className = 'dot dot--yellow';
      if (state) {
        state.textContent = 'Disconnected';
        state.style.color = 'var(--warning)';
      }
    }
  }

  async function fetchLiveChannels() {
    var fetchFn = window.apiFetch || window.fetch;
    try {
      var resp = await fetchFn('/api/echenon/live');
      if (!resp.ok) return;
      var data = await resp.json();

      if (data.event_count !== undefined) {
        var eventsEl = document.getElementById('echenon-events-count');
        if (eventsEl) eventsEl.textContent = Number(data.event_count).toLocaleString();
      }

      if (data.channels && Array.isArray(data.channels)) {
        for (var i = 0; i < data.channels.length; i++) {
          var chData = data.channels[i];
          var chNum = chData.ch !== undefined ? chData.ch : i;
          var minEl = document.getElementById('echenon-ch' + chNum + '-min');
          var meanEl = document.getElementById('echenon-ch' + chNum + '-mean');
          var maxEl = document.getElementById('echenon-ch' + chNum + '-max');

          if (minEl) minEl.textContent = chData.min !== undefined ? chData.min : '-';
          if (meanEl) meanEl.textContent = chData.mean !== undefined ? chData.mean : '-';
          if (maxEl) maxEl.textContent = chData.max !== undefined ? chData.max : '-';
        }
      }
    } catch (err) {
      // live endpoint error non-fatal, handled quietly
    }
  }

  async function pollAll() {
    await fetchStatus();
    await fetchLiveChannels();
  }

  function startPolling() {
    if (pollTimer) return;
    pollAll();
    pollTimer = setInterval(function() {
      if (isTabActive && document.visibilityState === 'visible') {
        pollAll();
      }
    }, 2500);
  }

  function stopPolling() {
    if (pollTimer) {
      clearInterval(pollTimer);
      pollTimer = null;
    }
  }

  window.addEventListener('tab-changed', function(e) {
    if (e.detail === 'tab-echenon') {
      isTabActive = true;
      var container = document.getElementById('tab-echenon');
      if (container && !isInitialized) {
        isInitialized = true;
        renderScaffold(container);
      }
      startPolling();
    } else {
      isTabActive = false;
      stopPolling();
    }
  });

  document.addEventListener('visibilitychange', function() {
    if (document.visibilityState === 'visible' && isTabActive) {
      pollAll();
    }
  });

})();

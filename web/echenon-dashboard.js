// web/echenon-dashboard.js
// Echenon Device Status Dashboard
// Shows device status summary, live metrics via WebSocket, and ride page link.

(function() {
  'use strict';

  var isInitialized = false;
  var isTabActive = false;

  // --- WebSocket Feed State ---
  var ws = null;
  var wsReconnectTimer = null;
  var wsReconnectDelay = 1000;
  var MAX_RECONNECT_DELAY = 16000;
  var lastWsRx = 0;

  function formatDuration(seconds) {
    if (!seconds && seconds !== 0) return '-';
    var h = Math.floor(seconds / 3600);
    var m = Math.floor((seconds % 3600) / 60);
    if (h > 0) return h + 'h ' + m + 'm';
    return m + 'm';
  }

  // --- WebSocket Live Feed ---
  function getWsUrl() {
    var protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    return protocol + '//' + window.location.host + '/ws/echenon/live';
  }

  function updateWsStatus(state, text) {
    var dot = document.getElementById('echenon-ws-dot');
    var label = document.getElementById('echenon-ws-status');
    if (!dot || !label) return;

    dot.className = 'dot';
    if (state === 'connected') {
      dot.classList.add('dot--green');
      label.textContent = text || 'Live';
      label.style.color = 'var(--success)';
    } else if (state === 'connecting' || state === 'reconnecting') {
      dot.classList.add('dot--yellow');
      label.textContent = text || 'Connecting...';
      label.style.color = 'var(--warning)';
    } else {
      dot.classList.add('dot--red');
      label.textContent = text || 'Disconnected';
      label.style.color = 'var(--error)';
    }
  }

  function connectWS() {
    if (!isTabActive) return;
    if (ws && (ws.readyState === WebSocket.OPEN || ws.readyState === WebSocket.CONNECTING)) {
      return;
    }

    if (wsReconnectTimer) {
      clearTimeout(wsReconnectTimer);
      wsReconnectTimer = null;
    }

    updateWsStatus('connecting', 'Connecting...');

    try {
      ws = new WebSocket(getWsUrl());
    } catch (err) {
      scheduleWsReconnect();
      return;
    }

    ws.onopen = function() {
      wsReconnectDelay = 1000;
      updateWsStatus('connected', 'Live');
      try { ws.send('ping'); } catch(e) {}
    };

    ws.onmessage = function(e) {
      lastWsRx = Date.now();
      try {
        var data = JSON.parse(e.data);
        handleLiveMessage(data);
      } catch (err) {
        // parse error — non-fatal
      }
    };

    ws.onerror = function() {
      // Browser invokes onclose immediately after onerror
    };

    ws.onclose = function() {
      ws = null;
      if (isTabActive) {
        scheduleWsReconnect();
      } else {
        updateWsStatus('disconnected', 'Disconnected');
      }
    };

    // 15s keepalive ping
    var keepalive = setInterval(function() {
      if (!ws || ws.readyState !== WebSocket.OPEN) { clearInterval(keepalive); return; }
      try { ws.send('ping'); } catch(e) { clearInterval(keepalive); }
    }, 15000);
  }

  function scheduleWsReconnect() {
    if (!isTabActive || wsReconnectTimer) return;
    updateWsStatus('reconnecting', 'Reconnecting (' + Math.round(wsReconnectDelay / 1000) + 's)...');
    wsReconnectTimer = setTimeout(function() {
      wsReconnectTimer = null;
      connectWS();
    }, wsReconnectDelay);
    wsReconnectDelay = Math.min(wsReconnectDelay * 2, MAX_RECONNECT_DELAY);
  }

  function disconnectWS() {
    if (wsReconnectTimer) {
      clearTimeout(wsReconnectTimer);
      wsReconnectTimer = null;
    }
    if (ws) {
      ws.onclose = null;
      ws.close();
      ws = null;
    }
    updateWsStatus('disconnected', 'Disconnected');
  }

  function handleLiveMessage(data) {
    var cadenceEl = document.getElementById('echenon-live-cadence');
    var powerEl = document.getElementById('echenon-live-power');
    var levelEl = document.getElementById('echenon-live-level');
    var hrEl = document.getElementById('echenon-live-hr');

    if (cadenceEl && data.cadence_rpm !== undefined) {
      cadenceEl.textContent = Number(data.cadence_rpm).toFixed(0);
    }
    if (powerEl && data.power_watts !== undefined) {
      powerEl.textContent = Number(data.power_watts).toFixed(0);
    }
    if (levelEl && data.level_echelon !== undefined) {
      levelEl.textContent = data.level_echelon;
    }
    if (hrEl && data.heart_rate !== undefined) {
      hrEl.textContent = data.heart_rate;
    }
    if (data.session_state) {
      updateSessionUI(data.session_state);
    }
  }

  // --- Scaffold Render ---
  function renderScaffold(container) {
    // Feather icon paths (24×24 viewBox, stroke-only, 2px)
    var iconActivity = '<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="22 12 18 12 15 21 9 3 6 12 2 12"></polyline></svg>';
    var iconArrowRight = '<svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><line x1="5" y1="12" x2="19" y2="12"></line><polyline points="12 5 19 12 12 19"></polyline></svg>';

    var html =
      // --- Live Metrics Card ---
      '<div class="card" style="margin-bottom:1rem;">' +
        '<div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:1rem;">' +
          '<h3 style="margin:0; font-size:1.15rem; font-weight:700;">Live Metrics</h3>' +
          '<div style="display:flex; align-items:center; gap:0.5rem;">' +
            '<span class="dot dot--yellow" id="echenon-ws-dot"></span>' +
            '<span id="echenon-ws-status" style="font-size:0.85rem; font-weight:700; text-transform:uppercase;">Connecting...</span>' +
          '</div>' +
        '</div>' +
        '<div style="display:grid; grid-template-columns:repeat(2, 1fr); gap:0.75rem;">' +
          '<div style="background:var(--bg); border:1px solid var(--border); border-radius:10px; padding:0.75rem; text-align:center;">' +
            '<div style="font-size:0.75rem; font-weight:600; text-transform:uppercase; color:var(--text-muted); margin-bottom:0.25rem;">Cadence</div>' +
            '<div style="font-size:1.75rem; font-weight:800; font-family:monospace; line-height:1.1; color:var(--primary);" id="echenon-live-cadence">--</div>' +
            '<div style="font-size:0.7rem; color:var(--text-muted); margin-top:0.15rem;">RPM</div>' +
          '</div>' +
          '<div style="background:var(--bg); border:1px solid var(--border); border-radius:10px; padding:0.75rem; text-align:center;">' +
            '<div style="font-size:0.75rem; font-weight:600; text-transform:uppercase; color:var(--text-muted); margin-bottom:0.25rem;">Power</div>' +
            '<div style="font-size:1.75rem; font-weight:800; font-family:monospace; line-height:1.1; color:var(--primary);" id="echenon-live-power">--</div>' +
            '<div style="font-size:0.7rem; color:var(--text-muted); margin-top:0.15rem;">Watts</div>' +
          '</div>' +
          '<div style="background:var(--bg); border:1px solid var(--border); border-radius:10px; padding:0.75rem; text-align:center;">' +
            '<div style="font-size:0.75rem; font-weight:600; text-transform:uppercase; color:var(--text-muted); margin-bottom:0.25rem;">Resistance</div>' +
            '<div style="font-size:1.75rem; font-weight:800; font-family:monospace; line-height:1.1; color:var(--primary);" id="echenon-live-level">--</div>' +
            '<div style="font-size:0.7rem; color:var(--text-muted); margin-top:0.15rem;">Level</div>' +
          '</div>' +
          '<div style="background:var(--bg); border:1px solid var(--border); border-radius:10px; padding:0.75rem; text-align:center;">' +
            '<div style="font-size:0.75rem; font-weight:600; text-transform:uppercase; color:var(--text-muted); margin-bottom:0.25rem;">Heart Rate</div>' +
            '<div style="font-size:1.75rem; font-weight:800; font-family:monospace; line-height:1.1; color:var(--primary);" id="echenon-live-hr">--</div>' +
            '<div style="font-size:0.7rem; color:var(--text-muted); margin-top:0.15rem;">BPM</div>' +
          '</div>' +
        '</div>' +
      '</div>' +

      // --- Echenon Status Card ---
      '<div class="card" style="margin-bottom:1rem;">' +
        '<h3 style="margin:0 0 0.75rem 0; font-size:1.15rem; font-weight:700;">Echenon Status</h3>' +
        '<div class="info-row">' +
          '<span class="text-muted">Bike Connection</span>' +
          '<div style="display:flex; align-items:center; gap:0.4rem;">' +
            '<span class="dot dot--grey" id="echenon-bike-dot"></span>' +
            '<span id="echenon-bike-status" style="font-weight:600;">Not connected</span>' +
          '</div>' +
        '</div>' +
        '<div class="info-row">' +
          '<span class="text-muted">Calibration Status</span>' +
          '<div id="echenon-calibration-container" style="display:flex; align-items:center; gap:0.4rem;">' +
            '<span id="echenon-calibration-status" style="font-weight:600;">Checking...</span>' +
            '<a href="#" id="echenon-calibration-link" style="display:none; color:var(--primary); font-size:0.85rem; text-decoration:none; font-weight:600;">Calibrate &rarr;</a>' +
          '</div>' +
        '</div>' +
        '<div class="info-row">' +
          '<span class="text-muted">Session State</span>' +
          '<span id="echenon-session-status" style="font-weight:600;">Idle</span>' +
        '</div>' +
      '</div>' +

      // --- Last Ride Card ---
      '<div class="card" style="margin-bottom:1rem;">' +
        '<h3 style="margin:0 0 0.75rem 0; font-size:1.15rem; font-weight:700;">Last Ride</h3>' +
        '<div id="echenon-last-ride-content">' +
          '<p style="color:var(--text-muted); font-size:0.9rem; margin:0;">No ride history available</p>' +
        '</div>' +
      '</div>' +

      // --- Open Ride Page Button ---
      '<a href="/ride" style="display:block; text-decoration:none;">' +
        '<div class="card" style="margin-bottom:1rem; border-color:var(--primary); cursor:pointer; transition:transform 0.15s ease, box-shadow 0.15s ease;" ' +
            'onmouseenter="this.style.transform=\'translateY(-2px)\';this.style.boxShadow=\'0 4px 12px rgba(0,0,0,.1)\';" ' +
            'onmouseleave="this.style.transform=\'none\';this.style.boxShadow=\'none\';">' +
          '<div style="display:flex; align-items:center; justify-content:center; gap:0.75rem; padding:0.5rem 0;">' +
            iconActivity +
            '<span style="font-size:1.1rem; font-weight:700; color:var(--primary);">Open Ride Page</span>' +
            iconArrowRight +
          '</div>' +
        '</div>' +
      '</a>';

    container.innerHTML = html;

    var calLink = document.getElementById('echenon-calibration-link');
    if (calLink) {
      calLink.onclick = function(e) {
        e.preventDefault();
        var navSystem = document.querySelector('.tab-link[data-target="tab-system"]') || document.getElementById('nav-system');
        if (navSystem) {
          navSystem.click();
          setTimeout(function() {
            var pill = document.querySelector('.pill-btn[data-target="tab-level-mapping"]') || document.getElementById('pill-level-mapping');
            if (pill) pill.click();
          }, 50);
        }
      };
    }
  }

  // --- Data Fetching ---
  async function fetchBikeStatus() {
    var fetchFn = window.apiFetch || window.fetch;
    try {
      var resp = await fetchFn('/api/ble/status');
      if (!resp.ok) return;
      var data = await resp.json();
      updateBikeUI(data);
    } catch (err) {
      // non-fatal
    }
  }

  function updateBikeUI(data) {
    var dot = document.getElementById('echenon-bike-dot');
    var label = document.getElementById('echenon-bike-status');
    if (!dot || !label) return;

    dot.className = 'dot';
    if (data && (data.isScanning || (data.discovery && data.discovery.active))) {
      dot.classList.add('dot--yellow');
      label.textContent = 'Scanning...';
      label.style.color = 'var(--warning)';
    } else {
      var isConnected = false;
      if (data && data.connections && data.connections.length > 0) {
        isConnected = true;
      } else if (data && data.devices && data.devices.length > 0) {
        for (var i = 0; i < data.devices.length; i++) {
          if (data.devices[i].connected) {
            isConnected = true;
            break;
          }
        }
      }
      if (isConnected) {
        dot.classList.add('dot--green');
        label.textContent = 'Connected to Echelon EX-3';
        label.style.color = 'var(--success)';
      } else {
        dot.classList.add('dot--grey');
        label.textContent = 'Not connected';
        label.style.color = 'var(--text-muted)';
      }
    }
  }

  async function fetchCalibrationStatus() {
    var fetchFn = window.apiFetch || window.fetch;
    try {
      var resp = await fetchFn('/api/echenon/calibration/status');
      if (!resp.ok) return;
      var data = await resp.json();
      updateCalibrationUI(data);
    } catch (err) {
      // non-fatal
    }
  }

  function updateCalibrationUI(data) {
    var label = document.getElementById('echenon-calibration-status');
    var link = document.getElementById('echenon-calibration-link');
    if (!label) return;

    var nonZeroCount = 0;
    if (data && Array.isArray(data.table)) {
      for (var i = 0; i < data.table.length; i++) {
        if (data.table[i] > 0) nonZeroCount++;
      }
    }

    if (nonZeroCount > 0) {
      label.textContent = 'Calibrated (32/32 levels)';
      label.style.color = 'var(--success)';
      if (link) link.style.display = 'none';
    } else {
      label.textContent = 'Not calibrated';
      label.style.color = 'var(--warning)';
      if (link) link.style.display = 'inline';
    }
  }

  async function fetchSessionStatus() {
    var fetchFn = window.apiFetch || window.fetch;
    try {
      var resp = await fetchFn('/api/session/status');
      if (!resp.ok) return;
      var data = await resp.json();
      updateSessionUI(data.state);
    } catch (err) {
      // non-fatal
    }
  }

  function updateSessionUI(state) {
    var label = document.getElementById('echenon-session-status');
    if (!label || !state) return;
    var s = String(state).toUpperCase();
    if (s === 'ACTIVE') {
      label.textContent = 'Active';
      label.style.color = 'var(--success)';
    } else if (s === 'PAUSED') {
      label.textContent = 'Paused';
      label.style.color = 'var(--warning)';
    } else if (s === 'ENDED') {
      label.textContent = 'Ended';
      label.style.color = 'var(--text-muted)';
    } else {
      label.textContent = 'Idle';
      label.style.color = 'var(--text-muted)';
    }
  }

  async function fetchLastRide() {
    var fetchFn = window.apiFetch || window.fetch;
    try {
      var resp = await fetchFn('/api/session/history');
      if (!resp.ok) return;
      var data = await resp.json();

      var contentEl = document.getElementById('echenon-last-ride-content');
      if (!contentEl) return;

      var rides = data.rides || data.sessions || data;
      if (!Array.isArray(rides) || rides.length === 0) return;

      var ride = rides[0]; // Most recent
      var dateStr = '-';
      if (ride.start_epoch_s) {
        var d = new Date(ride.start_epoch_s * 1000);
        dateStr = d.toLocaleDateString(undefined, { month: 'short', day: 'numeric', year: 'numeric' });
      } else if (ride.date || ride.started_at || ride.timestamp) {
        var d = new Date(ride.date || ride.started_at || ride.timestamp);
        dateStr = d.toLocaleDateString(undefined, { month: 'short', day: 'numeric', year: 'numeric' });
      }
      var duration = ride.duration_s || ride.duration || 0;

      contentEl.innerHTML =
        '<div class="info-row">' +
          '<span class="text-muted">Date</span>' +
          '<span style="font-weight:600;">' + dateStr + '</span>' +
        '</div>' +
        '<div class="info-row">' +
          '<span class="text-muted">Duration</span>' +
          '<span style="font-family:monospace; font-weight:600;">' + formatDuration(duration) + '</span>' +
        '</div>';
    } catch (err) {
      console.warn('[echenon-dashboard] fetchLastRide error:', err);
    }
  }

  async function fetchAll() {
    await fetchBikeStatus();
    await fetchCalibrationStatus();
    await fetchSessionStatus();
    await fetchLastRide();
  }

  // --- Tab Lifecycle ---
  window.addEventListener('tab-changed', function(e) {
    if (e.detail === 'tab-echenon') {
      isTabActive = true;
      var container = document.getElementById('tab-echenon');
      if (container && !isInitialized) {
        isInitialized = true;
        renderScaffold(container);
      }
      fetchAll();
      connectWS();
      // Fallback: if WS stalls, fetch via HTTP
      if (!window._echenonFallback) {
        window._echenonFallback = setInterval(function() {
          if (isTabActive && lastWsRx > 0 && (Date.now() - lastWsRx) > 2500) {
            fetchAll();
          }
        }, 2500);
      }
    } else {
      isTabActive = false;
      disconnectWS();
    }
  });

  document.addEventListener('visibilitychange', function() {
    if (document.visibilityState === 'visible' && isTabActive) {
      fetchAll();
      connectWS();
    } else if (document.visibilityState === 'hidden') {
      disconnectWS();
    }
  });

})();

// web/calibration-wizard.js
// Echenon Passive Position Calibration Wizard
// Guides the user through levels 1–32 recording wiper counts at each level.

(function() {
  'use strict';

  var isInitialized = false;
  var isTabActive = false;
  var pollInterval = null;

  var wizardState = {
    active: false,
    step: 0,
    complete: false,
    pos_done: false,
    table: []
  };

  function getApiFetch() {
    return window.apiFetch || window.fetch;
  }

  async function fetchStatus() {
    try {
      var fetchFn = getApiFetch();
      var resp = await fetchFn('/api/echenon/calibration/status');
      if (!resp.ok) return;
      var data = await resp.json();
      wizardState = data;
      renderState();
    } catch (err) {
      console.error('Failed to fetch calibration status', err);
    }
  }

  async function startCalibration() {
    try {
      var fetchFn = getApiFetch();
      var resp = await fetchFn('/api/echenon/calibration/start', {
        method: 'POST'
      });
      if (resp.ok) {
        await fetchStatus();
      }
    } catch (err) {
      console.error('Failed to start calibration', err);
    }
  }

  async function stopCalibration() {
    try {
      var fetchFn = getApiFetch();
      var resp = await fetchFn('/api/echenon/calibration/stop', {
        method: 'POST'
      });
      if (resp.ok) {
        await fetchStatus();
      }
    } catch (err) {
      console.error('Failed to stop calibration', err);
    }
  }

  async function recordCurrentLevel() {
    if (!wizardState.active || wizardState.step < 1 || wizardState.step > 32) return;
    try {
      var fetchFn = getApiFetch();
      var resp = await fetchFn('/api/echenon/calibration/record-level', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ level: wizardState.step })
      });
      if (resp.ok) {
        await fetchStatus();
      }
    } catch (err) {
      console.error('Failed to record level', err);
    }
  }

  function renderScaffold(container) {
    container.innerHTML =
      '<div class="card" style="margin-bottom:1rem;">' +
        '<div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:0.75rem;">' +
          '<h3 style="margin:0; font-size:1.15rem; font-weight:700;">Passive Position Calibration</h3>' +
          '<span id="cal-wiz-status-badge" class="badge">IDLE</span>' +
        '</div>' +
        '<p class="text-muted" style="margin-top:0; font-size:0.9rem;">' +
          'Passive calibration maps the potentiometer wiper counts to each of the 32 console resistance levels.' +
        '</p>' +
        '<div id="cal-wiz-progress-container" style="display:none; margin-bottom:1rem;">' +
          '<div style="display:flex; justify-content:space-between; font-size:0.85rem; margin-bottom:0.25rem;">' +
            '<span class="text-muted">Progress</span>' +
            '<span id="cal-wiz-progress-text" style="font-weight:700;">Step 0 / 32</span>' +
          '</div>' +
          '<div style="height:8px; background:var(--bg-subtle, #e0e0e0); border-radius:4px; overflow:hidden;">' +
            '<div id="cal-wiz-progress-bar" style="height:100%; width:0%; background:var(--primary, #2196F3); transition:width 0.3s ease;"></div>' +
          '</div>' +
        '</div>' +
        '<div id="cal-wiz-action-area" style="padding:1rem 0 0.5rem 0;"></div>' +
      '</div>' +
      '<div class="card">' +
        '<h4 style="margin:0 0 0.75rem 0; font-size:1rem; font-weight:700;">Recorded Wiper Table</h4>' +
        '<div style="max-height:360px; overflow-y:auto; border:1px solid var(--border-color, #eee); border-radius:4px;">' +
          '<table style="width:100%; border-collapse:collapse; font-size:0.85rem;">' +
            '<thead>' +
              '<tr style="background:var(--bg-subtle, #f5f5f5); border-bottom:1px solid var(--border-color, #eee); text-align:left;">' +
                '<th style="padding:0.5rem 0.75rem;">Level</th>' +
                '<th style="padding:0.5rem 0.75rem; text-align:right;">Wiper Count</th>' +
                '<th style="padding:0.5rem 0.75rem; text-align:center;">Status</th>' +
              '</tr>' +
            '</thead>' +
            '<tbody id="cal-wiz-table-body"></tbody>' +
          '</table>' +
        '</div>' +
      '</div>';
  }

  function renderState() {
    var badge = document.getElementById('cal-wiz-status-badge');
    var progressContainer = document.getElementById('cal-wiz-progress-container');
    var progressText = document.getElementById('cal-wiz-progress-text');
    var progressBar = document.getElementById('cal-wiz-progress-bar');
    var actionArea = document.getElementById('cal-wiz-action-area');
    var tableBody = document.getElementById('cal-wiz-table-body');

    if (!badge || !actionArea || !tableBody) return;

    // Update Status Badge & Progress
    if (wizardState.active) {
      badge.textContent = 'RECORDING (STEP ' + wizardState.step + '/32)';
      badge.className = 'badge badge--amber';
      if (progressContainer) progressContainer.style.display = 'block';
      if (progressText) progressText.textContent = 'Step ' + (wizardState.step - 1) + ' / 32';
      if (progressBar) progressBar.style.width = Math.round(((wizardState.step - 1) / 32) * 100) + '%';
    } else if (wizardState.complete || wizardState.pos_done) {
      badge.textContent = 'COMPLETED';
      badge.className = 'badge badge--green';
      if (progressContainer) progressContainer.style.display = 'block';
      if (progressText) progressText.textContent = '32 / 32 Complete';
      if (progressBar) progressBar.style.width = '100%';
    } else {
      badge.textContent = 'IDLE';
      badge.className = 'badge';
      if (progressContainer) progressContainer.style.display = 'none';
    }

    // Update Action Area
    if (wizardState.active) {
      actionArea.innerHTML =
        '<div style="background:var(--bg-subtle, #f8f9fa); border-left:4px solid var(--primary, #2196F3); padding:0.75rem 1rem; margin-bottom:1rem; border-radius:2px;">' +
          '<div style="font-size:0.95rem; font-weight:700; margin-bottom:0.25rem;">Step ' + wizardState.step + ': Set Console to Level ' + wizardState.step + '</div>' +
          '<div class="text-muted" style="font-size:0.85rem;">Turn the bike\'s resistance knob until the console or app shows Level ' + wizardState.step + '. Allow the motor to settle, then click Record.</div>' +
        '</div>' +
        '<div style="display:flex; gap:0.5rem; align-items:center;">' +
          '<button id="cal-wiz-record-btn" class="btn btn--primary" style="font-weight:700;">Record Level ' + wizardState.step + '</button>' +
          '<button id="cal-wiz-stop-btn" class="btn btn--outline" style="color:var(--danger, #f44336);">Cancel</button>' +
        '</div>';

      var recordBtn = document.getElementById('cal-wiz-record-btn');
      if (recordBtn) {
        recordBtn.onclick = function() {
          recordBtn.disabled = true;
          recordCurrentLevel();
        };
      }
      var stopBtn = document.getElementById('cal-wiz-stop-btn');
      if (stopBtn) {
        stopBtn.onclick = function() {
          stopBtn.disabled = true;
          stopCalibration();
        };
      }
    } else if (wizardState.complete || wizardState.pos_done) {
      actionArea.innerHTML =
        '<div style="background:var(--bg-subtle, #e8f5e9); border-left:4px solid var(--success, #4caf50); padding:0.75rem 1rem; margin-bottom:1rem; border-radius:2px;">' +
          '<div style="font-size:0.95rem; font-weight:700; color:var(--success, #2e7d32); margin-bottom:0.25rem;">Calibration Complete</div>' +
          '<div class="text-muted" style="font-size:0.85rem;">All 32 levels have been recorded and saved to NVS. The unit is now calibrated.</div>' +
        '</div>' +
        '<button id="cal-wiz-start-btn" class="btn btn--outline">Recalibrate (Start Over)</button>';

      var restartBtn = document.getElementById('cal-wiz-start-btn');
      if (restartBtn) {
        restartBtn.onclick = function() {
          restartBtn.disabled = true;
          startCalibration();
        };
      }
    } else {
      actionArea.innerHTML =
        '<button id="cal-wiz-start-btn" class="btn btn--primary" style="font-weight:700;">Start Passive Calibration</button>';

      var startBtn = document.getElementById('cal-wiz-start-btn');
      if (startBtn) {
        startBtn.onclick = function() {
          startBtn.disabled = true;
          startCalibration();
        };
      }
    }

    // Render Table Rows
    var rowsHtml = '';
    var tableData = wizardState.table || [];
    for (var l = 1; l <= 32; l++) {
      var wiperVal = (tableData.length >= l) ? tableData[l - 1] : 0;
      var isRecorded = wiperVal > 0;
      var isCurrent = wizardState.active && (wizardState.step === l);

      var rowStyle = '';
      if (isCurrent) {
        rowStyle = 'background:rgba(33, 150, 243, 0.08); font-weight:700;';
      }

      rowsHtml +=
        '<tr style="border-bottom:1px solid var(--border-color, #eee); ' + rowStyle + '">' +
          '<td style="padding:0.4rem 0.75rem;">Level ' + l + (isCurrent ? ' <span class="badge badge--amber" style="font-size:0.7rem; padding:1px 4px;">CURRENT</span>' : '') + '</td>' +
          '<td style="padding:0.4rem 0.75rem; text-align:right; font-family:monospace;">' + (isRecorded ? wiperVal : '-') + '</td>' +
          '<td style="padding:0.4rem 0.75rem; text-align:center;">' +
            (isRecorded ? '<span style="color:var(--success, #4caf50);">✓</span>' : '<span class="text-muted">-</span>') +
          '</td>' +
        '</tr>';
    }
    tableBody.innerHTML = rowsHtml;
  }

  function startPolling() {
    stopPolling();
    fetchStatus();
    pollInterval = setInterval(function() {
      if (isTabActive && document.visibilityState === 'visible') {
        fetchStatus();
      }
    }, 2000);
  }

  function stopPolling() {
    if (pollInterval) {
      clearInterval(pollInterval);
      pollInterval = null;
    }
  }

  window.addEventListener('tab-changed', function(e) {
    if (e.detail === 'tab-calibration') {
      isTabActive = true;
      var container = document.getElementById('tab-calibration');
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
      fetchStatus();
    }
  });

})();

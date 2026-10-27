// web/calibration-wizard.js
// Echenon Level Mapping — Passive Position Calibration Wizard
// Pill panel under Setup tab. Guides the user through levels 1–32
// recording wiper counts at each level.

(function() {
  'use strict';

  var TAB_ID = 'tab-level-mapping';
  var isInitialized = false;
  var isTabActive = false;
  var pollInterval = null;
  var resetArmed = false;
  var resetTimer = null;

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

  async function resetCalibration() {
    try {
      var fetchFn = getApiFetch();
      var resp = await fetchFn('/api/echenon/calibration/stop', {
        method: 'POST'
      });
      if (resp.ok) {
        await fetchStatus();
      }
    } catch (err) {
      console.error('Failed to reset calibration', err);
    }
  }

  function disarmReset() {
    resetArmed = false;
    if (resetTimer) {
      clearTimeout(resetTimer);
      resetTimer = null;
    }
    var btn = document.getElementById('cal-wiz-reset-btn');
    if (btn) {
      btn.textContent = 'Reset to Defaults';
      btn.style.background = '';
      btn.style.color = 'var(--danger, #c62828)';
      btn.style.borderColor = 'var(--danger, #c62828)';
    }
  }

  function armReset() {
    var btn = document.getElementById('cal-wiz-reset-btn');
    if (!btn) return;
    if (!resetArmed) {
      resetArmed = true;
      btn.textContent = '\u26A0 Confirm Reset? (3s)';
      btn.style.background = 'var(--danger, #c62828)';
      btn.style.color = '#fff';
      btn.style.borderColor = 'var(--danger, #c62828)';
      resetTimer = setTimeout(disarmReset, 3000);
    } else {
      disarmReset();
      resetCalibration();
    }
  }

  function renderScaffold(container) {
    container.innerHTML =
      // Help text card
      '<div style="background:var(--bg-card, #fff); border:1px solid var(--border, #e0e0e0); border-radius:16px; padding:1.25rem; margin-bottom:1rem;">' +
        '<div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:0.75rem;">' +
          '<h3 style="margin:0; font-size:1.15rem; font-weight:700; color:var(--text, #212121);">Level Mapping</h3>' +
          '<span id="cal-wiz-status-badge" style="display:inline-block; padding:2px 10px; font-size:0.75rem; font-weight:700; border-radius:20px; background:var(--border, #e0e0e0); color:var(--text-secondary, #666);">IDLE</span>' +
        '</div>' +
        '<p style="margin:0 0 0.75rem 0; font-size:0.85rem; color:var(--text-secondary, #666); line-height:1.5;">' +
          'Level mapping calibrates the potentiometer to each of the 32 console resistance levels. ' +
          'During calibration, turn the bike\u2019s resistance knob to each level shown, allow the motor to settle, then record the position. ' +
          'This builds a lookup table so the device accurately tracks your resistance setting.' +
        '</p>' +
        '<div id="cal-wiz-progress-container" style="display:none; margin-bottom:1rem;">' +
          '<div style="display:flex; justify-content:space-between; font-size:0.85rem; margin-bottom:0.25rem;">' +
            '<span style="color:var(--text-secondary, #666);">Progress</span>' +
            '<span id="cal-wiz-progress-text" style="font-weight:700; color:var(--text, #212121);">Step 0 / 32</span>' +
          '</div>' +
          '<div style="height:8px; background:var(--border, #e0e0e0); border-radius:6px; overflow:hidden;">' +
            '<div id="cal-wiz-progress-bar" style="height:100%; width:0%; background:var(--primary, #2196F3); transition:width 0.3s ease;"></div>' +
          '</div>' +
        '</div>' +
        '<div id="cal-wiz-action-area" style="padding:0.5rem 0 0;"></div>' +
      '</div>' +
      // Table card
      '<div style="background:var(--bg-card, #fff); border:1px solid var(--border, #e0e0e0); border-radius:16px; padding:1.25rem;">' +
        '<div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:0.75rem;">' +
          '<h4 style="margin:0; font-size:1rem; font-weight:700; color:var(--text, #212121);">Recorded Wiper Table</h4>' +
          '<button id="cal-wiz-reset-btn" type="button" style="padding:4px 12px; font-size:0.8rem; font-weight:600; border:1px solid var(--danger, #c62828); border-radius:10px; background:transparent; color:var(--danger, #c62828); cursor:pointer;">Reset to Defaults</button>' +
        '</div>' +
        '<div id="cal-wiz-table-wrapper" style="max-height:360px; overflow-y:auto; border:1px solid var(--border, #e0e0e0); border-radius:10px;">' +
          '<table style="width:100%; border-collapse:collapse; font-size:0.85rem;">' +
            '<thead>' +
              '<tr style="background:var(--bg-card, #fafafa); border-bottom:1px solid var(--border, #e0e0e0); text-align:left;">' +
                '<th style="padding:0.5rem 0.75rem; color:var(--text-secondary, #666); font-weight:600;">Level</th>' +
                '<th style="padding:0.5rem 0.75rem; text-align:right; color:var(--text-secondary, #666); font-weight:600;">Wiper Count</th>' +
                '<th style="padding:0.5rem 0.75rem; text-align:center; color:var(--text-secondary, #666); font-weight:600;">Status</th>' +
              '</tr>' +
            '</thead>' +
            '<tbody id="cal-wiz-table-body"></tbody>' +
          '</table>' +
        '</div>' +
      '</div>';

    var resetBtn = document.getElementById('cal-wiz-reset-btn');
    if (resetBtn) {
      resetBtn.onclick = armReset;
    }
  }

  function renderEmptyState() {
    var actionArea = document.getElementById('cal-wiz-action-area');
    if (!actionArea) return;
    actionArea.innerHTML =
      '<div style="text-align:center; padding:1.5rem 1rem; color:var(--text-secondary, #666);">' +
        '<p style="margin:0 0 0.75rem 0; font-size:0.9rem;">No calibration data yet. Run the calibration wizard to map all 32 resistance levels.</p>' +
        '<button id="cal-wiz-start-btn" type="button" style="padding:8px 20px; font-size:0.9rem; font-weight:700; border:none; border-radius:10px; background:var(--primary, #2196F3); color:#fff; cursor:pointer;">Start Level Mapping</button>' +
      '</div>';

    var startBtn = document.getElementById('cal-wiz-start-btn');
    if (startBtn) {
      startBtn.onclick = function() {
        startBtn.disabled = true;
        startCalibration();
      };
    }
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
      badge.style.background = 'rgba(249, 168, 37, 0.15)';
      badge.style.color = '#f9a825';
      if (progressContainer) progressContainer.style.display = 'block';
      if (progressText) progressText.textContent = 'Step ' + (wizardState.step - 1) + ' / 32';
      if (progressBar) progressBar.style.width = Math.round(((wizardState.step - 1) / 32) * 100) + '%';
    } else if (wizardState.complete || wizardState.pos_done) {
      badge.textContent = 'COMPLETED';
      badge.style.background = 'rgba(46, 125, 50, 0.15)';
      badge.style.color = 'var(--success, #2e7d32)';
      if (progressContainer) progressContainer.style.display = 'block';
      if (progressText) progressText.textContent = '32 / 32 Complete';
      if (progressBar) progressBar.style.width = '100%';
    } else {
      badge.textContent = 'IDLE';
      badge.style.background = 'var(--border, #e0e0e0)';
      badge.style.color = 'var(--text-secondary, #666)';
      if (progressContainer) progressContainer.style.display = 'none';
    }

    // Check for empty state (no data, not active, not complete)
    var hasData = false;
    var tableData = wizardState.table || [];
    for (var i = 0; i < tableData.length; i++) {
      if (tableData[i] > 0) { hasData = true; break; }
    }

    if (!wizardState.active && !wizardState.complete && !wizardState.pos_done && !hasData) {
      renderEmptyState();
    } else if (wizardState.active) {
      // Active calibration UI
      actionArea.innerHTML =
        '<div style="background:rgba(33, 150, 243, 0.06); border-left:4px solid var(--primary, #2196F3); padding:0.75rem 1rem; margin-bottom:1rem; border-radius:2px;">' +
          '<div style="font-size:0.95rem; font-weight:700; margin-bottom:0.25rem; color:var(--text, #212121);">Step ' + wizardState.step + ': Set Console to Level ' + wizardState.step + '</div>' +
          '<div style="font-size:0.85rem; color:var(--text-secondary, #666);">Turn the bike\u2019s resistance knob until the console or app shows Level ' + wizardState.step + '. Allow the motor to settle, then click Record.</div>' +
        '</div>' +
        '<div style="display:flex; gap:0.5rem; align-items:center;">' +
          '<button id="cal-wiz-record-btn" type="button" style="padding:8px 20px; font-size:0.9rem; font-weight:700; border:none; border-radius:10px; background:var(--primary, #2196F3); color:#fff; cursor:pointer;">Record Level ' + wizardState.step + '</button>' +
          '<button id="cal-wiz-stop-btn" type="button" style="padding:8px 16px; font-size:0.9rem; font-weight:600; border:1px solid var(--danger, #c62828); border-radius:10px; background:transparent; color:var(--danger, #c62828); cursor:pointer;">Cancel</button>' +
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
        '<div style="background:rgba(46, 125, 50, 0.06); border-left:4px solid var(--success, #2e7d32); padding:0.75rem 1rem; margin-bottom:1rem; border-radius:2px;">' +
          '<div style="font-size:0.95rem; font-weight:700; color:var(--success, #2e7d32); margin-bottom:0.25rem;">Calibration Complete</div>' +
          '<div style="font-size:0.85rem; color:var(--text-secondary, #666);">All 32 levels have been recorded and saved. The unit is now calibrated for accurate resistance tracking.</div>' +
        '</div>' +
        '<button id="cal-wiz-start-btn" type="button" style="padding:8px 16px; font-size:0.9rem; font-weight:600; border:1px solid var(--border, #e0e0e0); border-radius:10px; background:transparent; color:var(--text, #212121); cursor:pointer;">Recalibrate (Start Over)</button>';

      var restartBtn = document.getElementById('cal-wiz-start-btn');
      if (restartBtn) {
        restartBtn.onclick = function() {
          restartBtn.disabled = true;
          startCalibration();
        };
      }
    } else {
      // Has some data but not complete
      actionArea.innerHTML =
        '<button id="cal-wiz-start-btn" type="button" style="padding:8px 20px; font-size:0.9rem; font-weight:700; border:none; border-radius:10px; background:var(--primary, #2196F3); color:#fff; cursor:pointer;">Start Level Mapping</button>';

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
    for (var l = 1; l <= 32; l++) {
      var wiperVal = (tableData.length >= l) ? tableData[l - 1] : 0;
      var isRecorded = wiperVal > 0;
      var isCurrent = wizardState.active && (wizardState.step === l);

      var rowStyle = 'border-bottom:1px solid var(--border, #e0e0e0);';
      if (isCurrent) {
        rowStyle += ' background:rgba(33, 150, 243, 0.06); font-weight:700;';
      }

      rowsHtml +=
        '<tr style="' + rowStyle + '">' +
          '<td style="padding:0.4rem 0.75rem; color:var(--text, #212121);">Level ' + l +
            (isCurrent ? ' <span style="display:inline-block; padding:1px 6px; font-size:0.65rem; font-weight:700; border-radius:20px; background:rgba(249, 168, 37, 0.15); color:#f9a825;">CURRENT</span>' : '') +
          '</td>' +
          '<td style="padding:0.4rem 0.75rem; text-align:right; font-family:\'Courier New\', monospace; color:var(--text, #212121);">' + (isRecorded ? wiperVal : '-') + '</td>' +
          '<td style="padding:0.4rem 0.75rem; text-align:center;">' +
            (isRecorded ? '<span style="color:var(--success, #2e7d32);">\u2713</span>' : '<span style="color:var(--text-secondary, #666);">-</span>') +
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

  function onTabChanged(e) {
    var detail = e.detail;
    var tabId = (detail && detail.tab) ? detail.tab : detail;
    if (tabId === TAB_ID) {
      isTabActive = true;
      var container = document.getElementById(TAB_ID);
      if (container && !isInitialized) {
        isInitialized = true;
        renderScaffold(container);
      }
      startPolling();
    } else {
      isTabActive = false;
      stopPolling();
    }
  }

  window.addEventListener('tab-changed', onTabChanged);
  document.addEventListener('tab-changed', onTabChanged);

  document.addEventListener('visibilitychange', function() {
    if (document.visibilityState === 'visible' && isTabActive) {
      fetchStatus();
    }
  });

})();

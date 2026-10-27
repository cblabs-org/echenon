// web/recalibration.js
// Echenon Power Tuning — Recalibration & Power Table Management
// Pill panel under System tab. Supports individual power-point entry,
// points list, CSV upload, and active table preview.

(function() {
  'use strict';

  var TAB_ID = 'tab-power-tuning';
  var isInitialized = false;
  var isTabActive = false;
  var pollInterval = null;
  var resetArmed = false;
  var resetTimer = null;

  var pointsData = {
    points: [],
    count: 0,
    max: 100
  };

  var tablePreviewData = null;

  function getApiFetch() {
    return window.apiFetch || window.fetch;
  }

  async function fetchPoints() {
    try {
      var fetchFn = getApiFetch();
      var resp = await fetchFn('/api/echenon/recalibration/points');
      if (!resp.ok) return;
      pointsData = await resp.json();
      renderPointsList();
    } catch (err) {
      console.error('Failed to fetch recalibration points', err);
    }
  }

  async function fetchActiveTable() {
    var metaDiv = document.getElementById('recal-table-meta');
    try {
      var fetchFn = (typeof getApiFetch === 'function' ? getApiFetch() : null) || window.apiFetch || window.fetch;
      var resp = await fetchFn('/api/echenon/power-table/active');
      if (!resp.ok) {
        if (metaDiv) metaDiv.textContent = 'Could not load power table \u2014 check device connection.';
        return;
      }
      tablePreviewData = await resp.json();
      renderTablePreview();
    } catch (err) {
      console.error('Failed to fetch active power table', err);
      if (metaDiv) metaDiv.textContent = 'Could not load power table \u2014 check device connection.';
    }
  }

  async function addPoint(level, power) {
    try {
      var fetchFn = getApiFetch();
      var resp = await fetchFn('/api/echenon/recalibration/point', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ level: level, power: power })
      });
      if (resp.ok) {
        await fetchPoints();
        await fetchActiveTable();
        return true;
      }
      return false;
    } catch (err) {
      console.error('Failed to add recalibration point', err);
      return false;
    }
  }

  async function resetPoints() {
    try {
      var fetchFn = getApiFetch();
      var resp = await fetchFn('/api/echenon/recalibration/reset', {
        method: 'POST'
      });
      if (resp.ok) {
        await fetchPoints();
      }
    } catch (err) {
      console.error('Failed to reset recalibration points', err);
    }
  }

  async function uploadPowerTableCsv(csvContent) {
    try {
      var fetchFn = getApiFetch();
      var resp = await fetchFn('/api/echenon/power-table/upload', {
        method: 'POST',
        headers: { 'Content-Type': 'text/csv' },
        body: csvContent
      });
      var result = await resp.json();
      if (resp.ok && result.success) {
        setMsg('Power table uploaded successfully.', false);
        await fetchActiveTable();
      } else {
        setMsg('Upload failed: ' + (result.error || 'Unknown error'), true);
      }
    } catch (err) {
      console.error('Failed to upload power table', err);
      setMsg('Upload failed: network error', true);
    }
  }

  function setMsg(text, isError) {
    var el = document.getElementById('recal-msg');
    if (!el) return;
    el.textContent = text || '';
    el.style.color = isError ? 'var(--danger, #c62828)' : 'var(--success, #2e7d32)';
  }

  function disarmReset() {
    resetArmed = false;
    if (resetTimer) {
      clearTimeout(resetTimer);
      resetTimer = null;
    }
    var btn = document.getElementById('recal-reset-btn');
    if (btn) {
      btn.textContent = 'Reset to Defaults';
      btn.style.background = '';
      btn.style.color = 'var(--danger, #c62828)';
      btn.style.borderColor = 'var(--danger, #c62828)';
    }
  }

  function armReset() {
    var btn = document.getElementById('recal-reset-btn');
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
      resetPoints();
    }
  }

  function renderScaffold(container) {
    container.innerHTML =
      // Card 1: Add Point Form
      '<div style="background:var(--card-bg); border:1px solid var(--border, #e0e0e0); border-radius:16px; padding:1.25rem; margin-bottom:1rem;">' +
        '<div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:0.75rem;">' +
          '<h3 style="margin:0; font-size:1.15rem; font-weight:700; color:var(--text, #212121);">Power Tuning</h3>' +
          '<span id="recal-count-badge" style="display:inline-block; padding:2px 10px; font-size:0.75rem; font-weight:700; border-radius:20px; background:var(--border, #e0e0e0); color:var(--text-muted);">0 / 100 POINTS</span>' +
        '</div>' +
        '<p style="margin:0 0 0.75rem 0; font-size:0.85rem; color:var(--text-muted); line-height:1.5;">' +
          'Fine-tune the power curve by recording known power readings at specific resistance levels. ' +
          'Ride at a steady cadence with an external power meter, then enter the displayed console level and measured power. ' +
          'Each point refines the firmware\u2019s power estimation for more accurate watt values.' +
        '</p>' +
        '<form id="recal-add-form" style="display:grid; grid-template-columns:1fr 1fr; gap:0.75rem; margin-bottom:0.75rem;">' +
          '<div>' +
            '<label for="recal-level-input" style="display:block; font-size:0.8rem; font-weight:600; margin-bottom:0.25rem; color:var(--text-muted);">Level (1\u201332)</label>' +
            '<input id="recal-level-input" type="number" min="1" max="32" value="1" required style="width:100%; padding:8px 10px; border:1px solid var(--border, #e0e0e0); border-radius:10px; font-size:0.9rem; color:var(--text, #212121); background:var(--card-bg);" />' +
          '</div>' +
          '<div>' +
            '<label for="recal-power-input" style="display:block; font-size:0.8rem; font-weight:600; margin-bottom:0.25rem; color:var(--text-muted);">Power (Watts)</label>' +
            '<input id="recal-power-input" type="number" min="0" max="2500" step="0.5" placeholder="e.g. 150" required style="width:100%; padding:8px 10px; border:1px solid var(--border, #e0e0e0); border-radius:10px; font-size:0.9rem; color:var(--text, #212121); background:var(--card-bg);" />' +
          '</div>' +
          '<div style="grid-column: span 2; display:flex; gap:0.5rem; justify-content:flex-end; margin-top:0.25rem;">' +
            '<button type="button" id="recal-reset-btn" style="padding:6px 14px; font-size:0.85rem; font-weight:600; border:1px solid var(--danger, #c62828); border-radius:10px; background:transparent; color:var(--danger, #c62828); cursor:pointer;">Reset to Defaults</button>' +
            '<button type="submit" id="recal-submit-btn" style="padding:6px 20px; font-size:0.85rem; font-weight:700; border:none; border-radius:10px; background:var(--primary, #2196F3); color:#fff; cursor:pointer;">Record Point</button>' +
          '</div>' +
        '</form>' +
        '<div id="recal-msg" style="font-size:0.85rem; color:var(--text-muted); min-height:1.2em;"></div>' +
      '</div>' +

      // Card 2: Recorded Points List
      '<div style="background:var(--card-bg); border:1px solid var(--border, #e0e0e0); border-radius:16px; padding:1.25rem; margin-bottom:1rem;">' +
        '<h4 style="margin:0 0 0.75rem 0; font-size:1rem; font-weight:700; color:var(--text, #212121);">Stored Points</h4>' +
        '<div style="max-height:220px; overflow-y:auto; border:1px solid var(--border, #e0e0e0); border-radius:10px;">' +
          '<table style="width:100%; border-collapse:collapse; font-size:0.85rem;">' +
            '<thead>' +
              '<tr style="background:var(--card-bg); border-bottom:1px solid var(--border, #e0e0e0); text-align:left;">' +
                '<th style="padding:0.4rem 0.5rem; color:var(--text-muted); font-weight:600;">Level</th>' +
                '<th style="padding:0.4rem 0.5rem; text-align:right; color:var(--text-muted); font-weight:600;">Watts</th>' +
                '<th style="padding:0.4rem 0.5rem; text-align:right; color:var(--text-muted); font-weight:600;">Cadence</th>' +
                '<th style="padding:0.4rem 0.5rem; text-align:right; color:var(--text-muted); font-weight:600;">Wiper</th>' +
                '<th style="padding:0.4rem 0.5rem; text-align:right; color:var(--text-muted); font-weight:600;">Internal (i)</th>' +
              '</tr>' +
            '</thead>' +
            '<tbody id="recal-points-body"></tbody>' +
          '</table>' +
        '</div>' +
      '</div>' +

      // Card 3: Power Table CSV Upload
      '<div style="background:var(--card-bg); border:1px solid var(--border, #e0e0e0); border-radius:16px; padding:1.25rem; margin-bottom:1rem;">' +
        '<h4 style="margin:0 0 0.5rem 0; font-size:1rem; font-weight:700; color:var(--text, #212121);">Upload Power Table (CSV)</h4>' +
        '<p style="margin:0 0 0.75rem 0; font-size:0.85rem; color:var(--text-muted); line-height:1.5;">' +
          'Upload a 32-level \u00D7 31-cadence-bin CSV table (0\u2013150 RPM in 5 RPM increments, Tier 2). ' +
          'This replaces the currently active power lookup table.' +
        '</p>' +
        '<div style="display:flex; gap:0.5rem; align-items:center; flex-wrap:wrap;">' +
          '<input id="recal-csv-file" type="file" accept=".csv,text/csv" style="font-size:0.85rem; color:var(--text, #212121);" />' +
          '<button id="recal-csv-upload-btn" type="button" style="padding:6px 14px; font-size:0.85rem; font-weight:600; border:1px solid var(--border, #e0e0e0); border-radius:10px; background:transparent; color:var(--text, #212121); cursor:pointer;">Upload Table</button>' +
        '</div>' +
      '</div>' +

      // Card 4: Active Power Table Preview
      '<div style="background:var(--card-bg); border:1px solid var(--border, #e0e0e0); border-radius:16px; padding:1.25rem;">' +
        '<div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:0.75rem;">' +
          '<h4 style="margin:0; font-size:1rem; font-weight:700; color:var(--text, #212121);">Active Power Table</h4>' +
          '<div style="display:flex; gap:0.5rem; align-items:center;">' +
            '<span id="recal-table-tier-badge" style="display:inline-block; padding:2px 10px; font-size:0.75rem; font-weight:700; border-radius:20px; background:var(--border, #e0e0e0); color:var(--text-muted);">DEFAULT</span>' +
            '<button id="recal-refresh-table-btn" type="button" style="padding:2px 10px; font-size:0.75rem; font-weight:600; border:1px solid var(--border, #e0e0e0); border-radius:10px; background:transparent; color:var(--text-muted); cursor:pointer;">Refresh</button>' +
          '</div>' +
        '</div>' +
        '<div id="recal-table-meta" style="font-size:0.85rem; color:var(--text-muted); margin-bottom:0.5rem;">' +
          'Loading table details...' +
        '</div>' +
        '<div style="max-height:300px; overflow:auto; border:1px solid var(--border, #e0e0e0); border-radius:10px;">' +
          '<table id="recal-preview-table" style="width:100%; border-collapse:collapse; font-size:0.75rem; font-family:\'Courier New\', monospace;"></table>' +
        '</div>' +
      '</div>';

    // Form submit listener
    var form = document.getElementById('recal-add-form');
    if (form) {
      form.onsubmit = async function(evt) {
        evt.preventDefault();
        var lvlInput = document.getElementById('recal-level-input');
        var pwrInput = document.getElementById('recal-power-input');
        var submitBtn = document.getElementById('recal-submit-btn');

        if (!lvlInput || !pwrInput) return;
        var lvl = parseInt(lvlInput.value, 10);
        var pwr = parseFloat(pwrInput.value);

        if (isNaN(lvl) || isNaN(pwr)) return;

        if (submitBtn) submitBtn.disabled = true;
        var ok = await addPoint(lvl, pwr);
        if (submitBtn) submitBtn.disabled = false;

        if (ok) {
          pwrInput.value = '';
          setMsg('Point recorded.', false);
        } else {
          setMsg('Could not record point. Verify inputs or storage capacity.', true);
        }
      };
    }

    // Reset button listener
    var resetBtn = document.getElementById('recal-reset-btn');
    if (resetBtn) {
      resetBtn.onclick = armReset;
    }

    // Upload button listener
    var uploadBtn = document.getElementById('recal-csv-upload-btn');
    if (uploadBtn) {
      uploadBtn.onclick = function() {
        var fileInput = document.getElementById('recal-csv-file');
        if (!fileInput || !fileInput.files || fileInput.files.length === 0) {
          setMsg('Please select a CSV file to upload.', true);
          return;
        }
        var file = fileInput.files[0];
        var reader = new FileReader();
        reader.onload = function(e) {
          var contents = e.target.result;
          uploadPowerTableCsv(contents);
        };
        reader.readAsText(file);
      };
    }

    // Refresh button listener
    var refreshBtn = document.getElementById('recal-refresh-table-btn');
    if (refreshBtn) {
      refreshBtn.onclick = function() {
        fetchActiveTable();
      };
    }
  }

  function renderPointsList() {
    var countBadge = document.getElementById('recal-count-badge');
    var tbody = document.getElementById('recal-points-body');
    if (countBadge) {
      countBadge.textContent = pointsData.count + ' / ' + (pointsData.max || 100) + ' POINTS';
      if (pointsData.count > 0) {
        countBadge.style.background = 'rgba(46, 125, 50, 0.15)';
        countBadge.style.color = 'var(--success, #2e7d32)';
      } else {
        countBadge.style.background = 'var(--border, #e0e0e0)';
        countBadge.style.color = 'var(--text-muted)';
      }
    }
    if (!tbody) return;

    if (!pointsData.points || pointsData.points.length === 0) {
      tbody.innerHTML =
        '<tr><td colspan="5" style="text-align:center; padding:1.5rem 1rem; color:var(--text-muted);">' +
          'No power tuning points recorded yet. Ride with an external power meter and record reference points to refine watt accuracy.' +
        '</td></tr>';
      return;
    }

    var html = '';
    for (var i = 0; i < pointsData.points.length; i++) {
      var pt = pointsData.points[i];
      html +=
        '<tr style="border-bottom:1px solid var(--border, #e0e0e0);">' +
          '<td style="padding:0.4rem 0.5rem; font-weight:700; color:var(--text, #212121);">Level ' + pt.level + '</td>' +
          '<td style="padding:0.4rem 0.5rem; text-align:right; font-family:\'Courier New\', monospace; color:var(--primary, #2196F3); font-weight:700;">' + (pt.power ? pt.power.toFixed(1) : '0') + ' W</td>' +
          '<td style="padding:0.4rem 0.5rem; text-align:right; font-family:\'Courier New\', monospace; color:var(--text, #212121);">' + (pt.cadence ? pt.cadence.toFixed(1) : '0') + ' RPM</td>' +
          '<td style="padding:0.4rem 0.5rem; text-align:right; font-family:\'Courier New\', monospace; color:var(--text, #212121);">' + (pt.wiper || 0) + '</td>' +
          '<td style="padding:0.4rem 0.5rem; text-align:right; font-family:\'Courier New\', monospace; color:var(--text, #212121);">' + (pt.internal || 0) + '</td>' +
        '</tr>';
    }
    tbody.innerHTML = html;
  }

  function renderTablePreview() {
    var tierBadge = document.getElementById('recal-table-tier-badge');
    var metaDiv = document.getElementById('recal-table-meta');
    var tableElem = document.getElementById('recal-preview-table');

    if (!tablePreviewData) return;

    if (tierBadge) {
      var tierNames = { 1: 'TIER 1 (DEFAULT)', 2: 'TIER 2 (SELECTED/CUSTOM)', 3: 'TIER 3 (RECALIBRATED)' };
      tierBadge.textContent = tierNames[tablePreviewData.tier] || ('TIER ' + tablePreviewData.tier);
      if (tablePreviewData.tier === 3) {
        tierBadge.style.background = 'rgba(46, 125, 50, 0.15)';
        tierBadge.style.color = 'var(--success, #2e7d32)';
      } else if (tablePreviewData.tier === 2) {
        tierBadge.style.background = 'rgba(249, 168, 37, 0.15)';
        tierBadge.style.color = '#f9a825';
      } else {
        tierBadge.style.background = 'var(--border, #e0e0e0)';
        tierBadge.style.color = 'var(--text-muted)';
      }
    }

    if (metaDiv) {
      var idStr = tablePreviewData.id ? ('ID: "' + tablePreviewData.id + '" | ') : '';
      metaDiv.textContent = idStr + tablePreviewData.levels + ' levels \u00D7 ' + tablePreviewData.rpm_bins + ' cadence bins (0 to ' + tablePreviewData.rpm_max + ' RPM, step ' + tablePreviewData.rpm_step + ' RPM)';
    }

    if (!tableElem || !tablePreviewData.watts) return;

    // Header row with RPM bins
    var thead = '<thead><tr style="background:var(--card-bg); border-bottom:1px solid var(--border, #e0e0e0); position:sticky; top:0;">';
    thead += '<th style="padding:0.3rem 0.5rem; border-right:1px solid var(--border, #e0e0e0); color:var(--text-muted);">Lvl</th>';
    for (var b = 0; b < tablePreviewData.rpm_bins; b++) {
      var rpm = b * tablePreviewData.rpm_step;
      thead += '<th style="padding:0.3rem 0.4rem; text-align:right; color:var(--text-muted);">' + rpm + '</th>';
    }
    thead += '</tr></thead>';

    var tbody = '<tbody>';
    for (var l = 0; l < tablePreviewData.watts.length; l++) {
      var row = tablePreviewData.watts[l];
      tbody += '<tr style="border-bottom:1px solid var(--border, #e0e0e0);">';
      tbody += '<td style="padding:0.25rem 0.5rem; font-weight:700; background:var(--card-bg); border-right:1px solid var(--border, #e0e0e0); position:sticky; left:0; color:var(--text, #212121);">' + (l + 1) + '</td>';
      for (var r = 0; r < row.length; r++) {
        tbody += '<td style="padding:0.25rem 0.4rem; text-align:right; color:var(--text, #212121);">' + row[r] + '</td>';
      }
      tbody += '</tr>';
    }
    tbody += '</tbody>';

    tableElem.innerHTML = thead + tbody;
  }

  function startPolling() {
    stopPolling();
    fetchPoints();
    fetchActiveTable();
    pollInterval = setInterval(function() {
      if (isTabActive && document.visibilityState === 'visible') {
        fetchPoints();
      }
    }, 3000);
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
      fetchPoints();
      fetchActiveTable();
    }
  });

})();

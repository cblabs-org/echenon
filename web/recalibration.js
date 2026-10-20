// web/recalibration.js
// Echenon Power-Point Recalibration & Power Table Management UI
// Supports individual power-point entry, points list, CSV upload, and active table preview.

(function() {
  'use strict';

  var isInitialized = false;
  var isTabActive = false;
  var pollInterval = null;

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
    try {
      var fetchFn = getApiFetch();
      var resp = await fetchFn('/api/echenon/power-table/active');
      if (!resp.ok) return;
      tablePreviewData = await resp.json();
      renderTablePreview();
    } catch (err) {
      console.error('Failed to fetch active power table', err);
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
    if (!confirm('Are you sure you want to clear all recalibration points?')) return;
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
        alert('Power table uploaded successfully!');
        await fetchActiveTable();
      } else {
        alert('Upload failed: ' + (result.error || 'Unknown error'));
      }
    } catch (err) {
      console.error('Failed to upload power table', err);
      alert('Upload failed: network error');
    }
  }

  function renderScaffold(container) {
    container.innerHTML =
      // Card 1: Add Point Form
      '<div class="card" style="margin-bottom:1rem;">' +
        '<div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:0.75rem;">' +
          '<h3 style="margin:0; font-size:1.15rem; font-weight:700;">Add Power-Point Correction</h3>' +
          '<span id="recal-count-badge" class="badge">0 / 100 POINTS</span>' +
        '</div>' +
        '<p class="text-muted" style="margin-top:0; font-size:0.85rem;">' +
          'Ride at a steady cadence. Enter the displayed console level and power read from the Echelon app or console.' +
        '</p>' +
        '<form id="recal-add-form" style="display:grid; grid-template-columns:1fr 1fr; gap:0.75rem; margin-bottom:1rem;">' +
          '<div>' +
            '<label for="recal-level-input" style="display:block; font-size:0.85rem; font-weight:700; margin-bottom:0.25rem;">Level (1–32)</label>' +
            '<input id="recal-level-input" type="number" min="1" max="32" value="1" required style="width:100%; padding:0.4rem 0.5rem; border:1px solid var(--border-color, #ccc); border-radius:4px; font-size:0.9rem;" />' +
          '</div>' +
          '<div>' +
            '<label for="recal-power-input" style="display:block; font-size:0.85rem; font-weight:700; margin-bottom:0.25rem;">Power (Watts)</label>' +
            '<input id="recal-power-input" type="number" min="0" max="2500" step="0.5" placeholder="e.g. 150" required style="width:100%; padding:0.4rem 0.5rem; border:1px solid var(--border-color, #ccc); border-radius:4px; font-size:0.9rem;" />' +
          '</div>' +
          '<div style="grid-column: span 2; display:flex; gap:0.5rem; justify-content:flex-end; margin-top:0.25rem;">' +
            '<button type="button" id="recal-reset-btn" class="btn btn--outline" style="color:var(--danger, #f44336);">Reset All</button>' +
            '<button type="submit" id="recal-submit-btn" class="btn btn--primary" style="font-weight:700;">Record Point</button>' +
          '</div>' +
        '</form>' +
      '</div>' +

      // Card 2: Recorded Points List
      '<div class="card" style="margin-bottom:1rem;">' +
        '<h4 style="margin:0 0 0.75rem 0; font-size:1rem; font-weight:700;">Stored Points</h4>' +
        '<div style="max-height:220px; overflow-y:auto; border:1px solid var(--border-color, #eee); border-radius:4px;">' +
          '<table style="width:100%; border-collapse:collapse; font-size:0.85rem;">' +
            '<thead>' +
              '<tr style="background:var(--bg-subtle, #f5f5f5); border-bottom:1px solid var(--border-color, #eee); text-align:left;">' +
                '<th style="padding:0.4rem 0.5rem;">Level</th>' +
                '<th style="padding:0.4rem 0.5rem; text-align:right;">Watts</th>' +
                '<th style="padding:0.4rem 0.5rem; text-align:right;">Cadence</th>' +
                '<th style="padding:0.4rem 0.5rem; text-align:right;">Wiper</th>' +
                '<th style="padding:0.4rem 0.5rem; text-align:right;">Internal (i)</th>' +
              '</tr>' +
            '</thead>' +
            '<tbody id="recal-points-body"></tbody>' +
          '</table>' +
        '</div>' +
      '</div>' +

      // Card 3: Power Table CSV Upload
      '<div class="card" style="margin-bottom:1rem;">' +
        '<h4 style="margin:0 0 0.5rem 0; font-size:1rem; font-weight:700;">Upload Power Table (CSV)</h4>' +
        '<p class="text-muted" style="margin-top:0; font-size:0.85rem;">' +
          'Upload a 32-level × 31-cadence-bin CSV table (0–150 RPM in 5 RPM increments, Tier 2).' +
        '</p>' +
        '<div style="display:flex; gap:0.5rem; align-items:center; flex-wrap:wrap;">' +
          '<input id="recal-csv-file" type="file" accept=".csv,text/csv" style="font-size:0.85rem;" />' +
          '<button id="recal-csv-upload-btn" class="btn btn--outline">Upload Table</button>' +
        '</div>' +
      '</div>' +

      // Card 4: Active Power Table Preview
      '<div class="card">' +
        '<div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:0.75rem;">' +
          '<h4 style="margin:0; font-size:1rem; font-weight:700;">Active Power Table</h4>' +
          '<div style="display:flex; gap:0.5rem; align-items:center;">' +
            '<span id="recal-table-tier-badge" class="badge">DEFAULT</span>' +
            '<button id="recal-refresh-table-btn" class="btn btn--outline" style="padding:2px 8px; font-size:0.75rem;">Refresh</button>' +
          '</div>' +
        '</div>' +
        '<div id="recal-table-meta" class="text-muted" style="font-size:0.85rem; margin-bottom:0.5rem;">' +
          'Loading table details...' +
        '</div>' +
        '<div style="max-height:300px; overflow:auto; border:1px solid var(--border-color, #eee); border-radius:4px;">' +
          '<table id="recal-preview-table" style="width:100%; border-collapse:collapse; font-size:0.75rem; font-family:monospace;"></table>' +
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
        } else {
          alert('Could not record point. Please verify inputs or storage capacity.');
        }
      };
    }

    // Reset button listener
    var resetBtn = document.getElementById('recal-reset-btn');
    if (resetBtn) {
      resetBtn.onclick = function() {
        resetPoints();
      };
    }

    // Upload button listener
    var uploadBtn = document.getElementById('recal-csv-upload-btn');
    if (uploadBtn) {
      uploadBtn.onclick = function() {
        var fileInput = document.getElementById('recal-csv-file');
        if (!fileInput || !fileInput.files || fileInput.files.length === 0) {
          alert('Please select a CSV file to upload.');
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
      countBadge.className = pointsData.count > 0 ? 'badge badge--green' : 'badge';
    }
    if (!tbody) return;

    if (!pointsData.points || pointsData.points.length === 0) {
      tbody.innerHTML = '<tr><td colspan="5" class="text-muted" style="text-align:center; padding:1rem;">No recalibration points recorded yet.</td></tr>';
      return;
    }

    var html = '';
    for (var i = 0; i < pointsData.points.length; i++) {
      var pt = pointsData.points[i];
      html +=
        '<tr style="border-bottom:1px solid var(--border-color, #eee);">' +
          '<td style="padding:0.4rem 0.5rem; font-weight:700;">Level ' + pt.level + '</td>' +
          '<td style="padding:0.4rem 0.5rem; text-align:right; font-family:monospace; color:var(--primary, #2196F3); font-weight:700;">' + (pt.power ? pt.power.toFixed(1) : '0') + ' W</td>' +
          '<td style="padding:0.4rem 0.5rem; text-align:right; font-family:monospace;">' + (pt.cadence ? pt.cadence.toFixed(1) : '0') + ' RPM</td>' +
          '<td style="padding:0.4rem 0.5rem; text-align:right; font-family:monospace;">' + (pt.wiper || 0) + '</td>' +
          '<td style="padding:0.4rem 0.5rem; text-align:right; font-family:monospace;">' + (pt.internal || 0) + '</td>' +
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
        tierBadge.className = 'badge badge--green';
      } else if (tablePreviewData.tier === 2) {
        tierBadge.className = 'badge badge--amber';
      } else {
        tierBadge.className = 'badge';
      }
    }

    if (metaDiv) {
      var idStr = tablePreviewData.id ? ('ID: "' + tablePreviewData.id + '" | ') : '';
      metaDiv.textContent = idStr + tablePreviewData.levels + ' levels × ' + tablePreviewData.rpm_bins + ' cadence bins (0 to ' + tablePreviewData.rpm_max + ' RPM, step ' + tablePreviewData.rpm_step + ' RPM)';
    }

    if (!tableElem || !tablePreviewData.watts) return;

    // Header row with RPM bins
    var thead = '<thead><tr style="background:var(--bg-subtle, #f5f5f5); border-bottom:1px solid var(--border-color, #eee); position:sticky; top:0;">';
    thead += '<th style="padding:0.3rem 0.5rem; border-right:1px solid var(--border-color, #eee);">Lvl</th>';
    for (var b = 0; b < tablePreviewData.rpm_bins; b++) {
      var rpm = b * tablePreviewData.rpm_step;
      thead += '<th style="padding:0.3rem 0.4rem; text-align:right;">' + rpm + '</th>';
    }
    thead += '</tr></thead>';

    var tbody = '<tbody>';
    for (var l = 0; l < tablePreviewData.watts.length; l++) {
      var row = tablePreviewData.watts[l];
      tbody += '<tr style="border-bottom:1px solid var(--border-color, #eee);">';
      tbody += '<td style="padding:0.25rem 0.5rem; font-weight:700; background:var(--bg-subtle, #fafafa); border-right:1px solid var(--border-color, #eee); position:sticky; left:0;">' + (l + 1) + '</td>';
      for (var r = 0; r < row.length; r++) {
        tbody += '<td style="padding:0.25rem 0.4rem; text-align:right;">' + row[r] + '</td>';
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

  window.addEventListener('tab-changed', function(e) {
    if (e.detail === 'tab-recalibration') {
      isTabActive = true;
      var container = document.getElementById('tab-recalibration');
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
      fetchPoints();
      fetchActiveTable();
    }
  });

})();

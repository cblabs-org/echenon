/**
 * Echenon — Stored Rides Portal Pill
 *
 * Sub-pill under the Echenon tab.
 * Displays ride history with per-sink sync status, download, delete, and retry.
 *
 * Endpoints consumed:
 *   GET    /api/session/history
 *   GET    /api/session/download?id=<session_id>
 *   POST   /api/session/discard   {session_id}
 *   POST   /api/session/upload    {session_id, service_id}
 */
(function () {
    'use strict';

    var TAB_ID = 'tab-stored-rides';
    var initialized = false;

    // =========================================================================
    // Helpers
    // =========================================================================

    function formatDuration(seconds) {
        if (!seconds && seconds !== 0) return '-';
        var h = Math.floor(seconds / 3600);
        var m = Math.floor((seconds % 3600) / 60);
        var s = seconds % 60;
        if (h > 0) return h + 'h ' + m + 'm';
        return m + 'm ' + s + 's';
    }

    function formatDate(epochS) {
        if (!epochS) return '-';
        return new Date(epochS * 1000).toLocaleString();
    }

    function formatDistance(km) {
        if (km === undefined || km === null) return '-';
        return Number(km).toFixed(1) + ' km';
    }

    function syncStatusBadge(status) {
        var color, icon, text;
        switch ((status || '').toUpperCase()) {
            case 'COMPLETE':
                color = 'var(--success, #2e7d32)';
                icon = '✅';
                text = 'Synced';
                break;
            case 'PENDING':
                color = 'var(--warning, #f9a825)';
                icon = '⏳';
                text = 'Pending';
                break;
            case 'FAILED':
                color = 'var(--error, #c62828)';
                icon = '❌';
                text = 'Failed';
                break;
            default:
                color = 'var(--text-muted)';
                icon = '—';
                text = 'Not sent';
                break;
        }
        return '<span style="display:inline-flex;align-items:center;gap:0.25rem;font-size:0.8rem;color:' + color + ';">' +
               icon + ' ' + text + '</span>';
    }

    // =========================================================================
    // UI Building
    // =========================================================================

    function buildUI(container) {
        container.innerHTML = [
            '<div style="background:var(--card-bg); border:1px solid var(--border, #e0e0e0); border-radius:16px; padding:1.25rem;">',
            '  <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:0.75rem;">',
            '    <h3 style="margin:0; font-size:1.15rem; font-weight:700; color:var(--text, #212121);">Stored Rides</h3>',
            '    <button id="sr-btn-refresh" type="button" style="padding:6px 14px; font-size:0.8rem; font-weight:600; border:1px solid var(--border, #e0e0e0); border-radius:10px; background:transparent; color:var(--text, #212121); cursor:pointer;">Refresh</button>',
            '  </div>',
            '  <div id="sr-rides-list" style="font-size:0.85rem; color:var(--text-muted);"><p style="margin:0; text-align:center; padding:2rem 0;">Loading...</p></div>',
            '</div>'
        ].join('\n');

        document.getElementById('sr-btn-refresh').addEventListener('click', fetchRides);
    }

    function renderRides(rides) {
        var listEl = document.getElementById('sr-rides-list');
        if (!listEl) return;

        if (!rides || rides.length === 0) {
            listEl.innerHTML = '<p style="margin:0; text-align:center; padding:2rem 0; color:var(--text-muted);">No rides recorded yet</p>';
            return;
        }

        var html = '';
        for (var i = 0; i < rides.length; i++) {
            var ride = rides[i];
            html += renderRideCard(ride);
        }
        listEl.innerHTML = html;

        // Attach event listeners after rendering
        for (var j = 0; j < rides.length; j++) {
            attachRideEvents(rides[j]);
        }
    }

    function renderRideCard(ride) {
        var sid = ride.session_id;
        var dateStr = formatDate(ride.start_epoch_s);
        var durationStr = formatDuration(ride.duration_s || ride.active_duration_s);
        var avgPower = (ride.avg_power_w !== undefined) ? ride.avg_power_w + ' W' : '-';
        var avgCadence = (ride.avg_cadence_rpm !== undefined) ? ride.avg_cadence_rpm + ' RPM' : '-';
        var avgHr = (ride.avg_hr_bpm !== undefined && ride.avg_hr_bpm > 0) ? ride.avg_hr_bpm + ' BPM' : '-';
        var totalWork = (ride.total_work_kj !== undefined) ? ride.total_work_kj + ' kJ' : '-';

        var card = '<div style="background:var(--bg, #fafafa); border:1px solid var(--border, #e0e0e0); border-radius:12px; padding:1rem; margin-bottom:0.75rem;">';

        // Header row: date + actions
        card += '<div style="display:flex;justify-content:space-between;align-items:flex-start;margin-bottom:0.5rem;flex-wrap:wrap;gap:0.5rem;">';
        card += '<div style="font-weight:700; font-size:0.9rem; color:var(--text, #212121);">' + dateStr + '</div>';
        card += '<div style="display:flex;gap:0.4rem;flex-wrap:wrap;">';
        card += '<button data-action="download" data-sid="' + sid + '" style="padding:4px 10px;font-size:0.75rem;font-weight:600;border:1px solid var(--primary, #2196F3);border-radius:8px;background:transparent;color:var(--primary, #2196F3);cursor:pointer;">Download TCX</button>';
        card += '<button data-action="delete" data-sid="' + sid + '" style="padding:4px 10px;font-size:0.75rem;font-weight:600;border:1px solid var(--error, #c62828);border-radius:8px;background:transparent;color:var(--error, #c62828);cursor:pointer;">Delete</button>';
        card += '</div>';
        card += '</div>';

        // Stats grid
        card += '<div style="display:grid;grid-template-columns:repeat(auto-fit, minmax(100px, 1fr));gap:0.5rem;margin-bottom:0.5rem;">';
        card += statCell('Duration', durationStr);
        card += statCell('Avg Power', avgPower);
        card += statCell('Avg Cadence', avgCadence);
        card += statCell('Avg HR', avgHr);
        card += statCell('Work', totalWork);
        card += '</div>';

        // Per-sink sync status
        if (ride.sync && ride.sync.length > 0) {
            card += '<div style="border-top:1px solid var(--border, #e0e0e0);padding-top:0.5rem;margin-top:0.25rem;">';
            card += '<div style="font-size:0.7rem;font-weight:600;text-transform:uppercase;color:var(--text-muted);margin-bottom:0.35rem;">Sync Status</div>';
            for (var s = 0; s < ride.sync.length; s++) {
                var sink = ride.sync[s];
                card += renderSyncRow(sid, sink);
            }
            card += '</div>';
        }

        card += '</div>';
        return card;
    }

    function statCell(label, value) {
        return '<div style="text-align:center;">' +
               '<div style="font-size:0.65rem;font-weight:600;text-transform:uppercase;color:var(--text-muted);">' + label + '</div>' +
               '<div style="font-size:0.95rem;font-weight:700;color:var(--text, #212121);font-family:monospace;">' + value + '</div>' +
               '</div>';
    }

    function formatServiceLabel(serviceId) {
        var sid = (serviceId || '').toLowerCase();
        if (sid === 'strava') return 'Strava';
        if (sid === 'intervals' || sid === 'intervals_icu') return 'Intervals.icu';
        if (sid === 'wahoo') return 'Wahoo';
        return serviceId;
    }

    function renderSyncRow(sid, sink) {
        var serviceId = sink.service_id || '';
        var status = (sink.status || 'NONE').toUpperCase();
        var label = formatServiceLabel(serviceId);
        var row = '<div style="display:flex;align-items:center;justify-content:space-between;gap:0.5rem;padding:0.2rem 0;flex-wrap:wrap;">';

        // Service name + status badge
        row += '<div style="display:flex;align-items:center;gap:0.4rem;">';
        row += '<span style="font-size:0.8rem;font-weight:600;color:var(--text, #212121);">' + escapeHtml(label) + '</span>';
        row += syncStatusBadge(status);
        row += '</div>';

        // Actions: remote link + retry button
        row += '<div style="display:flex;align-items:center;gap:0.3rem;">';

        // Remote link if synced and remote_url available
        if (status === 'COMPLETE' && sink.remote_url) {
            row += '<a href="' + escapeHtml(sink.remote_url) + '" target="_blank" rel="noopener" style="font-size:0.75rem;color:var(--primary, #2196F3);text-decoration:none;">View ↗</a>';
        } else if (status === 'COMPLETE' && sink.remote_id) {
            // Construct URLs for known services
            var url = buildRemoteUrl(serviceId, sink.remote_id);
            if (url) {
                row += '<a href="' + escapeHtml(url) + '" target="_blank" rel="noopener" style="font-size:0.75rem;color:var(--primary, #2196F3);text-decoration:none;">View ↗</a>';
            }
        }

        // Retry button for failed or none
        if (status === 'FAILED' || status === 'NONE') {
            row += '<button data-action="retry" data-sid="' + sid + '" data-service="' + escapeHtml(serviceId) + '" style="padding:2px 8px;font-size:0.7rem;font-weight:600;border:1px solid var(--warning, #f9a825);border-radius:6px;background:transparent;color:var(--warning, #f9a825);cursor:pointer;">Retry</button>';
        }

        // Show error if failed
        if (status === 'FAILED' && sink.error) {
            row += '<span style="font-size:0.7rem;color:var(--error, #c62828);" title="' + escapeHtml(sink.error) + '">⚠</span>';
        }

        row += '</div>';
        row += '</div>';
        return row;
    }

    function buildRemoteUrl(serviceId, remoteId) {
        if (!remoteId) return '';
        var sid = serviceId.toLowerCase();
        if (sid === 'strava') {
            return 'https://www.strava.com/activities/' + remoteId;
        }
        if (sid === 'intervals' || sid === 'intervals_icu') {
            return 'https://intervals.icu/activities/' + remoteId;
        }
        if (sid === 'wahoo') {
            return 'https://api.wahooligan.com/v1/workouts/' + remoteId;
        }
        return '';
    }

    function escapeHtml(str) {
        if (!str) return '';
        return String(str)
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;');
    }

    // =========================================================================
    // Event Handlers
    // =========================================================================

    function attachRideEvents(ride) {
        var sid = ride.session_id;

        // Download buttons
        var dlBtns = document.querySelectorAll('[data-action="download"][data-sid="' + sid + '"]');
        for (var d = 0; d < dlBtns.length; d++) {
            dlBtns[d].addEventListener('click', function () {
                var id = this.getAttribute('data-sid');
                window.location = '/api/session/download?id=' + id;
            });
        }

        // Delete buttons
        var delBtns = document.querySelectorAll('[data-action="delete"][data-sid="' + sid + '"]');
        for (var del = 0; del < delBtns.length; del++) {
            delBtns[del].addEventListener('click', function () {
                var id = this.getAttribute('data-sid');
                if (!confirm('Delete this ride permanently?')) return;
                fetch('/api/session/discard', {
                    method: 'POST',
                    credentials: 'same-origin',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ session_id: Number(id) })
                })
                    .then(function (r) { return r.json(); })
                    .then(function (data) {
                        if (data && data.ok) {
                            fetchRides();
                        } else {
                            alert('Delete failed: ' + ((data && data.error) || 'Unknown error'));
                        }
                    })
                    .catch(function (err) {
                        alert('Error deleting ride: ' + err);
                    });
            });
        }

        // Retry buttons
        var retryBtns = document.querySelectorAll('[data-action="retry"][data-sid="' + sid + '"]');
        for (var r = 0; r < retryBtns.length; r++) {
            retryBtns[r].addEventListener('click', function () {
                var id = this.getAttribute('data-sid');
                var service = this.getAttribute('data-service');
                var btn = this;
                btn.disabled = true;
                btn.textContent = '...';
                fetch('/api/session/upload', {
                    method: 'POST',
                    credentials: 'same-origin',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ session_id: Number(id), service_id: service })
                })
                    .then(function (resp) { return resp.json(); })
                    .then(function (data) {
                        if (data && data.ok) {
                            btn.textContent = 'Queued';
                            btn.style.borderColor = 'var(--success, #2e7d32)';
                            btn.style.color = 'var(--success, #2e7d32)';
                            setTimeout(fetchRides, 2000);
                        } else {
                            btn.textContent = 'Retry';
                            btn.disabled = false;
                            alert('Retry failed: ' + ((data && data.error) || 'Unknown error'));
                        }
                    })
                    .catch(function (err) {
                        btn.textContent = 'Retry';
                        btn.disabled = false;
                        alert('Error retrying upload: ' + err);
                    });
            });
        }
    }

    // =========================================================================
    // Data Fetching
    // =========================================================================

    function fetchRides() {
        fetch('/api/session/history', { credentials: 'same-origin' })
            .then(function (r) { return r.json(); })
            .then(function (data) {
                var rides = data.rides || data.sessions || [];
                renderRides(rides);
            })
            .catch(function (err) {
                var listEl = document.getElementById('sr-rides-list');
                if (listEl) {
                    listEl.innerHTML = '<p style="margin:0;text-align:center;padding:2rem 0;color:var(--error, #c62828);">Failed to load rides: ' + err + '</p>';
                }
            });
    }

    // =========================================================================
    // Tab Lifecycle
    // =========================================================================

    function onTabChanged(e) {
        var detail = e.detail;
        var targetTab = (detail && detail.tab) ? detail.tab : detail;
        if (targetTab === TAB_ID) {
            var container = document.getElementById(TAB_ID);
            if (!container) return;
            if (!initialized) {
                buildUI(container);
                initialized = true;
            }
            fetchRides();
        }
    }

    document.addEventListener('tab-changed', onTabChanged);
    window.addEventListener('tab-changed', onTabChanged);

    // Eagerly init if already active
    var el = document.getElementById(TAB_ID);
    if (el && el.classList.contains('active')) {
        buildUI(el);
        initialized = true;
        fetchRides();
    }
})();

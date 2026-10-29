/**
 * Echenon Apps — Wahoo Cloud OAuth2 Configuration
 *
 * Pill panel under Apps tab (order 30).
 * Manages Wahoo Cloud API credentials, OAuth authorization, and auto-sync.
 *
 * Endpoints consumed:
 *   GET  /api/wahoo/status
 *   POST /api/wahoo/credentials
 *   GET  /api/wahoo/auth-url
 *   POST /api/wahoo/disconnect
 *   POST /api/session/config
 *   POST /api/upload/settings
 */
(function () {
    'use strict';

    var TAB_IDS = ['tab-wahoo-cloud', 'tab-wahoo'];
    var initialized = false;

    function getContainer() {
        for (var i = 0; i < TAB_IDS.length; i++) {
            var el = document.getElementById(TAB_IDS[i]);
            if (el) return el;
        }
        return null;
    }

    function buildUI(container) {
        container.innerHTML = [
            '<div style="background:var(--card-bg); border:1px solid var(--border, #e0e0e0); border-radius:16px; padding:1.25rem; margin-bottom:1rem;">',
            '  <h4 style="margin:0 0 0.5rem 0; font-size:1rem; font-weight:700; color:var(--text, #212121);">Setup Guide</h4>',
            '  <ol style="margin:0; padding-left:1.25rem; font-size:0.85rem; color:var(--text-muted); line-height:1.6;">',
            '    <li>Go to the <a href="https://developers.wahooligan.com" target="_blank" rel="noopener" style="color:var(--primary, #2196F3);">Wahoo Developer Portal</a> and create an application</li>',
            '    <li>Set <strong>Application Name</strong> to <em>Echenon</em> (or your preference)</li>',
            '    <li>Set <strong>Redirect URI</strong> to: <code id="ech-wahoo-callback-url" style="font-family:monospace; background:var(--border, #e0e0e0); padding:2px 6px; border-radius:4px;"></code></li>',
            '    <li>Set <strong>Scopes</strong> to request: <code>workouts_read workouts_write</code></li>',
            '    <li>Copy the <strong>Client ID</strong> and <strong>Client Secret</strong> from your application, enter them below, then click <strong>Save Credentials</strong></li>',
            '    <li>Click <strong>Connect with Wahoo</strong> to authorize your account</li>',
            '  </ol>',
            '</div>',
            '<div style="background:var(--card-bg); border:1px solid var(--border, #e0e0e0); border-radius:16px; padding:1.25rem; margin-bottom:1rem;">',
            '  <h4 style="margin:0 0 0.5rem 0; font-size:1rem; font-weight:700; color:var(--text, #212121);">Wahoo API Credentials</h4>',
            '  <p style="font-size:0.8rem;color:var(--text-muted);margin:0 0 0.75rem 0; line-height:1.5;">',
            '    Enter your Wahoo Cloud API Application Client ID and Client Secret. The secret is stored encrypted in NVS and is write-only.',
            '  </p>',
            '  <div style="display:grid;gap:0.75rem;max-width:420px;">',
            '    <div>',
            '      <label for="ech-wahoo-client-id" style="display:block;font-size:0.8rem;font-weight:600;margin-bottom:0.25rem;color:var(--text-muted);">Client ID</label>',
            '      <input type="text" id="ech-wahoo-client-id" placeholder="e.g. your_client_id" autocomplete="off" style="width:100%;padding:8px 10px;border:1px solid var(--border, #e0e0e0);border-radius:10px;font-size:0.9rem;color:var(--text, #212121);background:var(--card-bg); font-family:inherit;">',
            '    </div>',
            '    <div>',
            '      <label for="ech-wahoo-client-secret" style="display:block;font-size:0.8rem;font-weight:600;margin-bottom:0.25rem;color:var(--text-muted);">Client Secret (write-only)</label>',
            '      <input type="password" id="ech-wahoo-client-secret" placeholder="Enter new client secret" autocomplete="new-password" style="width:100%;padding:8px 10px;border:1px solid var(--border, #e0e0e0);border-radius:10px;font-size:0.9rem;color:var(--text, #212121);background:var(--card-bg); font-family:inherit;">',
            '    </div>',
            '    <div>',
            '      <button id="ech-btn-wahoo-save" type="button" style="padding:8px 20px; font-size:0.85rem; font-weight:700; border:none; border-radius:10px; background:var(--primary, #2196F3); color:#fff; cursor:pointer;">Save Credentials</button>',
            '    </div>',
            '  </div>',
            '  <div id="ech-wahoo-msg" style="font-size:0.85rem;color:var(--text-muted);margin-top:0.75rem;min-height:1.2em;"></div>',
            '</div>',
            '<div style="background:var(--card-bg); border:1px solid var(--border, #e0e0e0); border-radius:16px; padding:1.25rem;">',
            '  <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:0.75rem;">',
            '    <h3 style="margin:0; font-size:1.15rem; font-weight:700; color:var(--text, #212121);">Wahoo Cloud</h3>',
            '    <span id="ech-wahoo-badge" style="display:inline-block; padding:2px 10px; font-size:0.75rem; font-weight:700; border-radius:20px; background:var(--border, #e0e0e0); color:var(--text-muted);">Checking...</span>',
            '  </div>',
            '  <p style="margin:0 0 0.75rem 0; font-size:0.85rem; color:var(--text-muted); line-height:1.5;">',
            '    Sync completed workouts directly to Wahoo Cloud (SYSTM / ELEMNT).',
            '  </p>',
            '  <div id="ech-wahoo-details" style="font-size:0.85rem;color:var(--text-muted);margin-bottom:1rem;"></div>',
            '  <div style="display:flex;flex-wrap:wrap;gap:0.5rem;align-items:center;margin-bottom:0.5rem;">',
            '    <button id="ech-btn-wahoo-auth" type="button" disabled style="padding:8px 16px; font-size:0.85rem; font-weight:700; border:none; border-radius:10px; background:var(--primary, #2196F3); color:#fff; cursor:not-allowed; opacity:0.5;">Connect with Wahoo</button>',
            '    <button id="ech-btn-wahoo-disconnect" type="button" style="display:none; padding:8px 16px; font-size:0.85rem; font-weight:600; border:1px solid var(--danger, #c62828); border-radius:10px; background:transparent; color:var(--danger, #c62828); cursor:pointer;">Disconnect</button>',
            '    <button id="ech-btn-wahoo-refresh" type="button" style="padding:8px 16px; font-size:0.85rem; font-weight:600; border:1px solid var(--border, #e0e0e0); border-radius:10px; background:transparent; color:var(--text, #212121); cursor:pointer;">Refresh Status</button>',
            '  </div>',
            '  <div id="ech-wahoo-auth-note" style="font-size:0.8rem; color:var(--text-muted); margin-bottom:0.75rem;">Save your API credentials first</div>',
            '  <label id="ech-wahoo-autoupload-row" style="display:none;align-items:center;gap:0.5rem;margin-top:0.5rem;font-size:0.85rem;color:var(--text, #212121);">',
            '    <input type="checkbox" id="ech-wahoo-autoupload" checked>',
            '    <span>Auto-sync completed rides to Wahoo Cloud</span>',
            '  </label>',
            '</div>'
        ].join('\n');

        var host = window.location.hostname || 'echenon.local';
        var port = window.location.port ? (':' + window.location.port) : '';
        var proto = window.location.protocol || 'http:';
        var urlEl = document.getElementById('ech-wahoo-callback-url');
        if (urlEl) urlEl.textContent = proto + '//' + host + port + '/api/wahoo/callback';

        document.getElementById('ech-btn-wahoo-save').addEventListener('click', saveCredentials);
        document.getElementById('ech-btn-wahoo-auth').addEventListener('click', startOAuth);
        document.getElementById('ech-btn-wahoo-disconnect').addEventListener('click', disconnectWahoo);
        document.getElementById('ech-btn-wahoo-refresh').addEventListener('click', fetchWahooStatus);
        document.getElementById('ech-wahoo-autoupload').addEventListener('change', saveAutoUpload);
    }

    function setMsg(text) {
        var el = document.getElementById('ech-wahoo-msg');
        if (el) el.textContent = text || '';
    }

    function updateStatusUI(data) {
        if (!data) return;
        var badge = document.getElementById('ech-wahoo-badge');
        var details = document.getElementById('ech-wahoo-details');
        var clientIdInput = document.getElementById('ech-wahoo-client-id');
        var authBtn = document.getElementById('ech-btn-wahoo-auth');
        var authNote = document.getElementById('ech-wahoo-auth-note');
        var disconnectBtn = document.getElementById('ech-btn-wahoo-disconnect');
        var autouploadRow = document.getElementById('ech-wahoo-autoupload-row');

        var isAuth = !!data.authenticated;
        var isConfig = !!data.configured;
        var isTokenValid = (data.token_valid !== false);

        if (badge) {
            if (isAuth) {
                if (!isTokenValid) {
                    badge.textContent = 'Token Expired';
                    badge.style.background = 'rgba(198, 40, 40, 0.15)';
                    badge.style.color = 'var(--danger, #c62828)';
                } else if (data.athlete_name) {
                    badge.textContent = 'Connected as ' + data.athlete_name;
                    badge.style.background = 'rgba(46, 125, 50, 0.15)';
                    badge.style.color = 'var(--success, #2e7d32)';
                } else {
                    badge.textContent = 'Connected';
                    badge.style.background = 'rgba(46, 125, 50, 0.15)';
                    badge.style.color = 'var(--success, #2e7d32)';
                }
            } else if (isConfig) {
                badge.textContent = 'Not Connected';
                badge.style.background = 'rgba(249, 168, 37, 0.15)';
                badge.style.color = '#f9a825';
            } else {
                badge.textContent = 'Not Connected';
                badge.style.background = 'var(--border, #e0e0e0)';
                badge.style.color = 'var(--text-muted)';
            }
        }

        if (details) {
            var parts = [];
            if (isAuth) {
                if (data.athlete_name) parts.push('User: ' + data.athlete_name);
                if (data.client_id) parts.push('Client ID: ' + data.client_id);
                if (data.expires_at) {
                    var expDate = new Date(data.expires_at * 1000).toLocaleString();
                    parts.push(isTokenValid ? ('Token expires: ' + expDate) : ('Token expired: ' + expDate));
                } else {
                    parts.push('Token status: ' + (isTokenValid ? 'Valid' : 'Expired'));
                }
            } else if (isConfig) {
                if (data.client_id) parts.push('Configured Client ID: ' + data.client_id);
                parts.push('Status: Ready to connect');
            }
            details.textContent = parts.join(' | ');
        }

        if (clientIdInput && data.client_id && !clientIdInput.value) {
            clientIdInput.value = data.client_id;
        }

        if (authBtn) {
            if (isAuth && isTokenValid) {
                authBtn.style.display = 'none';
            } else {
                authBtn.style.display = 'inline-block';
                if (isConfig) {
                    authBtn.disabled = false;
                    authBtn.style.opacity = '1';
                    authBtn.style.cursor = 'pointer';
                    authBtn.textContent = (!isTokenValid && isAuth) ? 'Reconnect with Wahoo' : 'Connect with Wahoo';
                } else {
                    authBtn.disabled = true;
                    authBtn.style.opacity = '0.5';
                    authBtn.style.cursor = 'not-allowed';
                    authBtn.textContent = 'Connect with Wahoo';
                }
            }
        }

        if (authNote) {
            if (!isConfig) {
                authNote.style.display = 'block';
                authNote.textContent = 'Save your API credentials first';
            } else if (isAuth && !isTokenValid) {
                authNote.style.display = 'block';
                authNote.textContent = 'OAuth token has expired. Reconnect to renew authorization.';
            } else {
                authNote.style.display = 'none';
            }
        }

        if (disconnectBtn) {
            disconnectBtn.style.display = isAuth ? 'inline-block' : 'none';
        }

        if (autouploadRow) {
            autouploadRow.style.display = isAuth ? 'flex' : 'none';
        }

        var autoChk = document.getElementById('ech-wahoo-autoupload');
        if (autoChk && data.enabled !== undefined) {
            autoChk.checked = !!data.enabled;
        }
    }

    function fetchWahooStatus() {
        fetch('/api/wahoo/status', { credentials: 'same-origin' })
            .then(function (r) { return r.json(); })
            .then(updateStatusUI)
            .catch(function () {});
    }

    function saveCredentials() {
        var cid = (document.getElementById('ech-wahoo-client-id').value || '').trim();
        var secEl = document.getElementById('ech-wahoo-client-secret');
        var sec = (secEl.value || '').trim();
        if (!cid || !sec) {
            setMsg('Both Client ID and Client Secret are required.');
            return;
        }
        fetch('/api/wahoo/credentials', {
            method: 'POST',
            credentials: 'same-origin',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ client_id: cid, client_secret: sec })
        })
            .then(function (r) { return r.json(); })
            .then(function (data) {
                if (data && data.ok) {
                    secEl.value = '';
                    setMsg('Credentials saved to encrypted storage.');
                    fetchWahooStatus();
                } else {
                    setMsg('Failed: ' + ((data && data.error) || 'Unknown error'));
                }
            })
            .catch(function (err) {
                setMsg('Error saving credentials: ' + err);
            });
    }

    function startOAuth() {
        var host = window.location.hostname || 'echenon.local';
        var port = window.location.port ? (':' + window.location.port) : '';
        var proto = window.location.protocol || 'http:';
        var redirectUri = proto + '//' + host + port + '/api/wahoo/callback';
        fetch('/api/wahoo/auth-url?redirect_uri=' + encodeURIComponent(redirectUri), {
            credentials: 'same-origin'
        })
            .then(function (r) { return r.json(); })
            .then(function (data) {
                var target = data && (data.url || data.authorize_url);
                if (target) {
                    window.location.href = target;
                } else {
                    setMsg('Could not build authorize URL: ' + ((data && data.error) || 'Save credentials first'));
                }
            })
            .catch(function (err) {
                setMsg('Failed to request auth URL: ' + err);
            });
    }

    function disconnectWahoo() {
        fetch('/api/wahoo/disconnect', { method: 'POST', credentials: 'same-origin' })
            .then(function (r) { return r.json(); })
            .then(function () {
                setMsg('Disconnected from Wahoo Cloud.');
                fetchWahooStatus();
            })
            .catch(function (err) {
                setMsg('Error disconnecting: ' + err);
            });
    }

    function saveAutoUpload() {
        var enabled = document.getElementById('ech-wahoo-autoupload').checked;
        fetch('/api/session/config', {
            method: 'POST',
            credentials: 'same-origin',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ auto_upload_enabled: enabled, auto_sync_wahoo: enabled })
        })
            .then(function (r) { return r.json(); })
            .then(function () {
                setMsg('Auto-sync preference updated.');
            })
            .catch(function () {});

        fetch('/api/upload/settings', {
            method: 'POST',
            credentials: 'same-origin',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ auto_sync_wahoo: enabled, auto_upload_enabled: enabled })
        }).catch(function () {});
    }

    function onTabChanged(e) {
        var detail = e.detail;
        var targetTab = (detail && detail.tab) ? detail.tab : detail;
        if (TAB_IDS.indexOf(targetTab) !== -1 || targetTab === 'Wahoo Cloud' || targetTab === 'tab-apps') {
            var container = getContainer();
            if (!container) return;
            if (!initialized) {
                buildUI(container);
                initialized = true;
            }
            fetchWahooStatus();
        }
    }

    document.addEventListener('tab-changed', onTabChanged);
    window.addEventListener('tab-changed', onTabChanged);

    // Detect wahoo_error in hash or search query after OAuth redirect
    function checkWahooError() {
        var str = (window.location.hash || '') + '&' + (window.location.search || '');
        var match = /[?&#]wahoo_error=([^&#]*)/.exec(str);
        if (match && match[1]) {
            var errMsg = decodeURIComponent(match[1].replace(/\+/g, ' '));
            if (errMsg) {
                var container = getContainer();
                if (container && !initialized) {
                    buildUI(container);
                    initialized = true;
                }
                setMsg('Wahoo authorization failed: ' + errMsg);
            }
            var hash = window.location.hash || '';
            var idx = hash.indexOf('wahoo_error=');
            if (idx !== -1) {
                var cleanHash = hash.substring(0, hash.indexOf('?wahoo_error='));
                if (!cleanHash) {
                    cleanHash = hash.substring(0, hash.indexOf('&wahoo_error='));
                }
                if (cleanHash) {
                    window.location.hash = cleanHash;
                }
            }
        }
    }
    checkWahooError();

    var el = getContainer();
    if (el && (el.classList.contains('active') || el.classList.contains('tab-content--active'))) {
        buildUI(el);
        initialized = true;
        fetchWahooStatus();
    }
})();

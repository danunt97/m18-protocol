/* Anonyme Seitenstatistik für akkuscan.app – ohne Cookies, ohne IP-Adresse, ohne Besucher-ID.
 * Pro Seitenaufruf: Pfad, woher der Besuch kam (nur der Domainname), Gerätetyp, Browser, Sprache, utm_source. */
(function () {
  try {
    if (!/^(www\.)?akkuscan\.app$/.test(location.hostname) || navigator.webdriver) return;
    var ref = "";
    try { if (document.referrer) ref = new URL(document.referrer).hostname.replace(/^www\./, ""); } catch (e) {}
    var entry = !/(^|\.)akkuscan\.app$/.test(ref);
    if (!entry) ref = "";
    var ua = navigator.userAgent;
    var dev = /iPad|Tablet/.test(ua) || (/Macintosh/.test(ua) && navigator.maxTouchPoints > 1) ? "tablet"
            : /Mobi|Android|iPhone|iPod/.test(ua) ? "mobile" : "desktop";
    var br = /Bluefy/i.test(ua) ? "Bluefy" : /Edg(A|iOS)?\//.test(ua) ? "Edge" : /SamsungBrowser/.test(ua) ? "Samsung"
           : /Firefox|FxiOS/.test(ua) ? "Firefox" : /CriOS|Chrome/.test(ua) ? "Chrome" : /Safari/.test(ua) ? "Safari" : "Andere";
    var p = new URLSearchParams(location.search);
    fetch("https://seccxaldsjhukavoilnm.supabase.co/rest/v1/rpc/track_view", {
      method: "POST", keepalive: true,
      headers: { apikey: "sb_publishable_KSiLpnnPHzgOvS3cTPnl_A_OVknKkj1", "Content-Type": "application/json" },
      body: JSON.stringify({ p_path: location.pathname, p_referrer: ref, p_device: dev, p_browser: br,
        p_lang: (navigator.language || "").slice(0, 5), p_utm: p.get("utm_source") || p.get("ref") || "", p_entry: entry })
    }).catch(function () {});
  } catch (e) {}
})();

/* Shop-Einstellungen für akkuscan.app – nur hier ändern.
 * ebay:  Link zum eBay-Angebot. Solange leer, zeigen die Kauf-Knöpfe auf /modul/ und es erscheint „bald verfügbar“.
 * price: Preis, wie er auf allen Seiten angezeigt wird. */
window.AKKUSCAN_SHOP = {
  ebay: "",
  price: "50 €"
};

(function () {
  var shop = window.AKKUSCAN_SHOP;
  function apply() {
    document.querySelectorAll("[data-price]").forEach(function (e) { e.textContent = shop.price; });
    document.querySelectorAll("[data-ebay]").forEach(function (a) {
      if (shop.ebay) { a.href = shop.ebay; a.target = "_blank"; a.rel = "noopener"; }
      else if (a.getAttribute("href") === "#") a.href = "/modul/#kaufen";
    });
    document.querySelectorAll("[data-ebay-soon]").forEach(function (e) { e.hidden = !!shop.ebay; });
  }
  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", apply); else apply();
})();

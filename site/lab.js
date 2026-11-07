// SPDX-License-Identifier: GPL-3.0-only
// The atoms hold still for people who asked for less motion.
(function () {
  try {
    if (matchMedia("(prefers-reduced-motion: reduce)").matches)
      document.querySelectorAll("svg.atom").forEach(function (s) { if (s.pauseAnimations) s.pauseAnimations(); });
  } catch (e) {}
})();

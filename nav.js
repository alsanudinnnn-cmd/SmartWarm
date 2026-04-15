document.addEventListener("DOMContentLoaded", () => {
  const body = document.body;
  const reduceMotion = window.matchMedia("(prefers-reduced-motion: reduce)");
  const header = document.querySelector("[data-site-header]");
  const toggle = document.querySelector("[data-nav-toggle]");
  const nav = document.querySelector("[data-nav-menu]");

  body.classList.add("motion-enhanced");
  requestAnimationFrame(() => {
    body.classList.add("is-page-ready");
  });

  window.addEventListener("pageshow", () => {
    body.classList.remove("is-page-exiting");
    body.classList.add("is-page-ready");
  });

  const revealTargets = Array.from(new Set([
    ...document.querySelectorAll(".hero, .hero-copy, .hero-card, .panel, .feature-strip article, .results-hero, .podium-section, .contact-hero, .contact-container")
  ]));

  revealTargets.forEach((element, index) => {
    element.classList.add("reveal-on-scroll");
    element.style.setProperty("--reveal-delay", `${Math.min(index * 70, 240)}ms`);
  });

  if (reduceMotion.matches || !("IntersectionObserver" in window)) {
    revealTargets.forEach((element) => element.classList.add("is-visible"));
  } else {
    const revealObserver = new IntersectionObserver((entries) => {
      entries.forEach((entry) => {
        if (!entry.isIntersecting) return;
        entry.target.classList.add("is-visible");
        revealObserver.unobserve(entry.target);
      });
    }, {
      threshold: 0.14,
      rootMargin: "0px 0px -10% 0px"
    });

    revealTargets.forEach((element) => {
      revealObserver.observe(element);
    });
  }

  document.querySelectorAll("a[href]").forEach((link) => {
    link.addEventListener("click", (event) => {
      if (
        event.defaultPrevented ||
        event.button !== 0 ||
        event.metaKey ||
        event.ctrlKey ||
        event.shiftKey ||
        event.altKey
      ) {
        return;
      }

      const href = link.getAttribute("href");
      if (!href || href.startsWith("#")) return;
      if (link.target && link.target !== "_self") return;
      if (link.hasAttribute("download")) return;

      const destination = new URL(link.href, window.location.href);
      if (destination.origin !== window.location.origin) return;

      const currentPath = window.location.pathname.replace(/\/$/, "");
      const nextPath = destination.pathname.replace(/\/$/, "");
      if (currentPath === nextPath && destination.hash) return;
      if (reduceMotion.matches) return;

      event.preventDefault();
      body.classList.add("is-page-exiting");
      window.setTimeout(() => {
        window.location.href = destination.href;
      }, 180);
    });
  });

  if (header && toggle && nav) {
    function closeMenu() {
      header.classList.remove("is-nav-open");
      toggle.setAttribute("aria-expanded", "false");
    }

    function openMenu() {
      header.classList.add("is-nav-open");
      toggle.setAttribute("aria-expanded", "true");
    }

    toggle.addEventListener("click", () => {
      const isOpen = header.classList.contains("is-nav-open");
      if (isOpen) {
        closeMenu();
        return;
      }

      openMenu();
    });

    nav.querySelectorAll("a").forEach((link) => {
      link.addEventListener("click", () => {
        closeMenu();
      });
    });

    document.addEventListener("click", (event) => {
      if (!header.contains(event.target)) {
        closeMenu();
      }
    });

    document.addEventListener("keydown", (event) => {
      if (event.key === "Escape") {
        closeMenu();
      }
    });
  }
});

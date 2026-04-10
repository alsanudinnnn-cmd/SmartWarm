document.addEventListener("DOMContentLoaded", () => {
  const header = document.querySelector("[data-site-header]");
  const toggle = document.querySelector("[data-nav-toggle]");
  const nav = document.querySelector("[data-nav-menu]");

  if (!header || !toggle || !nav) {
    return;
  }

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
});

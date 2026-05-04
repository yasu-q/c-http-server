const root = document.documentElement;
const button = document.getElementById("theme-toggle");

function set_theme(theme) {
  if (theme === "light") {
    root.setAttribute("data-theme", "light");
  } else {
    root.removeAttribute("data-theme"); // default = dark 
  }

  if (button) {
    button.textContent = theme === "light" ? "dark" : "light";
  }
}

// toggle on button click
if (button) {
  button.addEventListener("click", () => {
    const current = root.getAttribute("data-theme") === "light" ? "light" : "dark";
    const next = current === "light" ? "dark" : "light";
    set_theme(next);
  });
}
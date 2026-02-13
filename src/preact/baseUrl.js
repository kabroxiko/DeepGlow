// Shared BASE_URL logic
export function getBaseUrl() {
  let BASE_URL = "";
  if (
    typeof window !== "undefined" &&
    (window.location.protocol === "file:" ||
      window.location.hostname === "localhost" ||
      window.location.hostname === "127.0.0.1")
  ) {
    BASE_URL = localStorage.getItem("BASE_URL") || "";
    if (!BASE_URL) {
      BASE_URL = window.prompt("Please enter the BASE_URL for local development (e.g. http://192.168.1.100):");
      if (BASE_URL) {
        localStorage.setItem("BASE_URL", BASE_URL);
      }
    }
  } else if (typeof window !== "undefined" && window.BASE_URL) {
    BASE_URL = window.BASE_URL;
  }
  return BASE_URL;
}

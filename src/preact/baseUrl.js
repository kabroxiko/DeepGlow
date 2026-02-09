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
  } else if (typeof window !== "undefined" && window.BASE_URL) {
    BASE_URL = window.BASE_URL;
  }
  return BASE_URL;
}

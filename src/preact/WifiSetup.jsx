// WiFi setup page as Preact component (initial stub)

import { useRef, useState } from "preact/hooks";

export function WifiSetup() {
  const [result, setResult] = useState(null);
  const [error, setError] = useState(null);
  const [loading, setLoading] = useState(false);
  const formRef = useRef(null);

  const handleSubmit = (e) => {
    e.preventDefault();
    setError(null);
    setResult(null);
    setLoading(true);
    const form = formRef.current;
    const data = new URLSearchParams();
    data.append("ssid", form.ssid.value);
    data.append("password", form.password.value);
    const xhr = new XMLHttpRequest();
    xhr.open("POST", form.action, true);
    xhr.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
    xhr.onload = function () {
      setLoading(false);
      if (xhr.status === 200) {
        setResult(xhr.responseText);
      } else {
        setError("WiFi setup failed. Please try again.");
      }
    };
    xhr.onerror = function () {
      setLoading(false);
      setError("Network error. Please try again.");
    };
    xhr.send(data.toString());
  };

  if (result) {
    // Show server response as HTML (as in original captive portal)
    return <div dangerouslySetInnerHTML={{ __html: result }} />;
  }

  return (
    <div
      className="container"
      style={{
        minHeight: "100vh",
        display: "flex",
        flexDirection: "column",
        justifyContent: "flex-start",
      }}
    >
      <section
        className="card"
        style={{ maxWidth: 400, margin: "12px auto 0 auto" }}
      >
        <h2 style={{ fontSize: "1.3em", marginBottom: 8, textAlign: "center" }}>
          🐠 Aquarium Control
        </h2>
        <div
          style={{
            fontWeight: 500,
            color: "var(--secondary-color)",
            textAlign: "center",
            marginBottom: 16,
          }}
        >
          WiFi Setup
        </div>
        <div
          className="instructions"
          style={{ color: "var(--text-secondary)", marginBottom: 16 }}
        >
          Enter your WiFi details to connect.
        </div>
        <form
          id="wifiForm"
          method="POST"
          action="/wifi"
          ref={formRef}
          onSubmit={handleSubmit}
        >
          <label htmlFor="ssid">WiFi SSID</label>
          <input
            className="text-input"
            name="ssid"
            id="ssid"
            type="text"
            required
            autoComplete="on"
            autoFocus
          />
          <label htmlFor="password">WiFi Password</label>
          <input
            className="text-input"
            name="password"
            id="password"
            type="password"
            autoComplete="on"
          />
          <button
            className="btn btn-primary"
            type="submit"
            disabled={loading}
            style={{ marginTop: 12 }}
          >
            {loading ? "Connecting..." : "Connect"}
          </button>
        </form>
        {loading && (
          <div className="spinner" style={{ margin: "1em auto 0 auto" }} />
        )}
        {error && (
          <div
            style={{ color: "#ff4466", marginTop: "1em", textAlign: "center" }}
          >
            {error}
          </div>
        )}
        <div
          className="footer"
          style={{ marginTop: 18, color: "#aaa", fontSize: "0.95em" }}
        >
          Aquarium LED Controller &copy; 2026
        </div>
      </section>
    </div>
  );
}

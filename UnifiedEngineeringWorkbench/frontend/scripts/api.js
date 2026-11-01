const api = {
  async request(path, options = {}) {
    const res = await fetch(path, {
      headers: { "Content-Type": "application/json", ...(options.headers || {}) },
      ...options
    });
    const text = await res.text();
    let data = null;
    try { data = text ? JSON.parse(text) : null; } catch (e) {
      throw new Error("Backend returned non-JSON: " + text.slice(0, 180));
    }
    if (!res.ok || (data && data.ok === false)) {
      const msg = (data && (data.error || data.message)) || res.statusText;
      const err = new Error(msg);
      err.payload = data;
      throw err;
    }
    return data;
  },
  health() { return this.request("/api/health"); },
  thermalSolve(body) { return this.request("/api/thermal/solve", { method: "POST", body: JSON.stringify(body) }); },
  thermalConvergence() { return this.request("/api/thermal/convergence", { method: "POST", body: "{}" }); },
  stl(content) { return this.request("/api/geometry/stl", { method: "POST", body: JSON.stringify({ content }) }); },
  csv(content) { return this.request("/api/geometry/csv", { method: "POST", body: JSON.stringify({ content }) }); },
  benchmark(body) { return this.request("/api/performance/benchmark", { method: "POST", body: JSON.stringify(body) }); },
  discover() { return this.request("/api/hardware/discover"); },
  hw(path, body) { return this.request("/api/hardware/" + path, { method: "POST", body: JSON.stringify(body) }); },
  backprop() { return this.request("/api/neural/backprop", { method: "POST", body: JSON.stringify({ steps: 100 }) }); }
};

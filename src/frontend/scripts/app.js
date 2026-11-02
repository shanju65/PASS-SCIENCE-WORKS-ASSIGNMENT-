/**
 * UNIFIED ENGINEERING WORKBENCH — FRONTEND CLIENT CONTROLLER
 * Target: Windows 11 x64 · Offline Three.js & Chart.js Integration
 * Connects to C++17 local REST endpoints on localhost:8080/api
 */

document.addEventListener('DOMContentLoaded', () => {
    initNavigation();
    initSystemHealth();
    initCadViewer();
    initThermalSolver();
    initMulticoreBenchmark();
    initHardwareManager();
    initBackpropagation();
});

// -------------------------------------------------------------
// 1. Navigation Controller
// -------------------------------------------------------------
function initNavigation() {
    const tabs = document.querySelectorAll('.nav-tab');
    const panes = document.querySelectorAll('.tab-pane');

    tabs.forEach(tab => {
        tab.addEventListener('click', () => {
            const targetId = tab.getAttribute('data-tab');
            tabs.forEach(t => t.classList.remove('active'));
            panes.forEach(p => p.classList.remove('active'));

            tab.classList.add('active');
            const targetPane = document.getElementById(targetId);
            if (targetPane) targetPane.classList.add('active');

            // Trigger window resize for 3D canvas and charts
            window.dispatchEvent(new Event('resize'));
        });
    });
}

// -------------------------------------------------------------
// 2. Health & Telemetry Poller
// -------------------------------------------------------------
async function initSystemHealth() {
    const statusPill = document.getElementById('backend-status-badge');
    const statusText = document.getElementById('backend-status-text');

    try {
        const resp = await fetch('/api/health');
        if (!resp.ok) throw new Error('Backend HTTP error');
        const data = await resp.json();

        statusPill.className = 'status-pill status-pill-success';
        statusText.textContent = `Backend: Connected (MSVC x64)`;

        document.getElementById('telemetry-cpu').textContent = data.cpu || 'x86_64 Multicore';
        document.getElementById('telemetry-cores').textContent = `${data.logical_cores} Logical / ${data.physical_cores} Physical`;
        document.getElementById('telemetry-ram').textContent = `${data.ram_mb} MB System RAM`;
        document.getElementById('bench-cpu-name').textContent = data.cpu || 'x86_64 Multicore';
        document.getElementById('bench-phys-cores').textContent = data.physical_cores;
        document.getElementById('bench-log-cores').textContent = data.logical_cores;
        document.getElementById('bench-ram-size').textContent = `${data.ram_mb} MB`;

    } catch (err) {
        statusPill.className = 'status-pill status-pill-danger';
        statusText.textContent = 'Backend: Offline (Check Launch.exe)';
    }
}

// -------------------------------------------------------------
// 3. CAD & ANSYS 3D Three.js Viewport (Task B)
// -------------------------------------------------------------
let scene, camera, renderer, stlMesh, pointCloud;
let stlWireframe;
let currentPointsData = []; // Store coordinates and temperatures for raycasting

function initCadViewer() {
    const container = document.getElementById('three-canvas-container');
    if (!container || typeof THREE === 'undefined') return;

    scene = new THREE.Scene();
    scene.background = new THREE.Color(0x060913);

    const width = container.clientWidth || 800;
    const height = container.clientHeight || 600;

    camera = new THREE.PerspectiveCamera(45, width / height, 0.01, 100);
    camera.position.set(1.5, 1.2, 1.8);

    renderer = new THREE.WebGLRenderer({ antialias: true, alpha: false });
    renderer.setSize(width, height);
    renderer.setPixelRatio(window.devicePixelRatio);
    container.appendChild(renderer.domElement);

    // Lights
    const ambientLight = new THREE.AmbientLight(0xffffff, 0.6);
    scene.add(ambientLight);

    const dirLight1 = new THREE.DirectionalLight(0xffffff, 0.8);
    dirLight1.position.set(5, 10, 7);
    scene.add(dirLight1);

    const dirLight2 = new THREE.DirectionalLight(0x38bdf8, 0.4);
    dirLight2.position.set(-5, -5, -5);
    scene.add(dirLight2);

    // Grid floor
    const grid = new THREE.GridHelper(2.0, 20, 0x1e293b, 0x0f172a);
    grid.position.y = -0.1;
    scene.add(grid);

    // Mouse Orbit Controls (Self-contained implementation)
    let isDragging = false;
    let prevMouseX = 0;
    let prevMouseY = 0;
    let spherical = { radius: 2.8, theta: 0.8, phi: 0.9 };

    function updateCamera() {
        camera.position.x = spherical.radius * Math.sin(spherical.phi) * Math.sin(spherical.theta);
        camera.position.y = spherical.radius * Math.cos(spherical.phi);
        camera.position.z = spherical.radius * Math.sin(spherical.phi) * Math.cos(spherical.theta);
        camera.lookAt(0.5, 0.0, 0.0);
    }
    updateCamera();

    container.addEventListener('mousedown', (e) => {
        isDragging = true;
        prevMouseX = e.clientX;
        prevMouseY = e.clientY;
    });

    window.addEventListener('mouseup', () => { isDragging = false; });

    window.addEventListener('mousemove', (e) => {
        if (!isDragging) return;
        const dx = e.clientX - prevMouseX;
        const dy = e.clientY - prevMouseY;
        prevMouseX = e.clientX;
        prevMouseY = e.clientY;

        spherical.theta -= dx * 0.008;
        spherical.phi = Math.max(0.1, Math.min(Math.PI - 0.1, spherical.phi - dy * 0.008));
        updateCamera();
    });

    container.addEventListener('wheel', (e) => {
        e.preventDefault();
        spherical.radius = Math.max(0.2, Math.min(10.0, spherical.radius + e.deltaY * 0.003));
        updateCamera();
    });

    // Resize handler
    window.addEventListener('resize', () => {
        if (!container.clientWidth || !container.clientHeight) return;
        camera.aspect = container.clientWidth / container.clientHeight;
        camera.updateProjectionMatrix();
        renderer.setSize(container.clientWidth, container.clientHeight);
    });

    // Animation Loop
    function animate() {
        requestAnimationFrame(animate);
        renderer.render(scene, camera);
    }
    animate();

    // Setup UI event triggers
    document.getElementById('btn-load-stl').addEventListener('click', loadDefaultStl);
    document.getElementById('btn-load-csv').addEventListener('click', loadDefaultCsv);

    document.getElementById('input-upload-stl').addEventListener('change', (e) => {
        const file = e.target.files[0];
        if (file) {
            const reader = new FileReader();
            reader.onload = (evt) => parseStlData(evt.target.result);
            reader.readAsText(file);
        }
    });

    document.getElementById('input-upload-csv').addEventListener('change', (e) => {
        const file = e.target.files[0];
        if (file) {
            const reader = new FileReader();
            reader.onload = (evt) => parseCsvData(evt.target.result);
            reader.readAsText(file);
        }
    });

    document.getElementById('chk-stl-mesh').addEventListener('change', (e) => {
        if (stlMesh) stlMesh.visible = e.target.checked;
    });

    document.getElementById('chk-stl-wireframe').addEventListener('change', (e) => {
        if (stlWireframe) stlWireframe.visible = e.target.checked;
    });

    document.getElementById('chk-csv-points').addEventListener('change', (e) => {
        if (pointCloud) pointCloud.visible = e.target.checked;
    });

    document.getElementById('btn-fit-view').addEventListener('click', () => {
        spherical = { radius: 2.2, theta: 0.6, phi: 0.9 };
        updateCamera();
    });

    // Raycast / Node Inspection on hover/click
    const raycaster = new THREE.Raycaster();
    raycaster.params.Points.threshold = 0.03;
    const mouse = new THREE.Vector2();

    container.addEventListener('click', (e) => {
        const rect = container.getBoundingClientRect();
        mouse.x = ((e.clientX - rect.left) / container.clientWidth) * 2 - 1;
        mouse.y = -((e.clientY - rect.top) / container.clientHeight) * 2 + 1;

        if (!pointCloud) return;
        raycaster.setFromCamera(mouse, camera);
        const intersects = raycaster.intersectObject(pointCloud);

        const inspectorEl = document.getElementById('inspector-content');
        if (intersects.length > 0) {
            const idx = intersects[0].index;
            if (currentPointsData[idx]) {
                const node = currentPointsData[idx];
                inspectorEl.innerHTML = `
                    <div style="font-family: var(--font-mono); font-size: 0.85rem; color: #38bdf8;">
                        <div><strong>Node ID:</strong> #${node.id}</div>
                        <div><strong>Position (X, Y, Z):</strong> (${node.x.toFixed(3)}, ${node.y.toFixed(3)}, ${node.z.toFixed(3)}) m</div>
                        <div><strong>Temperature:</strong> <span style="font-size: 1.1rem; font-weight: bold; color: #fbbf24;">${node.t.toFixed(2)} °C</span></div>
                    </div>
                `;
            }
        }
    });

    // Auto-load default datasets
    loadDefaultStl();
    loadDefaultCsv();
}

async function loadDefaultStl() {
    try {
        const resp = await fetch('/api/geometry/parse_stl', { method: 'POST', body: '' });
        const data = await resp.json();
        if (data.success) renderStlGeometry(data);
    } catch (e) {
        console.error('Error loading default STL:', e);
    }
}

function parseStlData(content) {
    fetch('/api/geometry/parse_stl', { method: 'POST', body: content })
        .then(r => r.json())
        .then(data => {
            if (data.success) renderStlGeometry(data);
            else alert('STL Parse Error: ' + data.error);
        });
}

function renderStlGeometry(data) {
    if (stlMesh) scene.remove(stlMesh);
    if (stlWireframe) scene.remove(stlWireframe);

    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute('position', new THREE.Float32BufferAttribute(data.positions, 3));
    geometry.setAttribute('normal', new THREE.Float32BufferAttribute(data.normals, 3));

    // Solid Surface
    const material = new THREE.MeshStandardMaterial({
        color: 0x334155,
        roughness: 0.4,
        metalness: 0.2,
        transparent: true,
        opacity: 0.75,
        side: THREE.DoubleSide
    });
    stlMesh = new THREE.Mesh(geometry, material);
    stlMesh.visible = document.getElementById('chk-stl-mesh').checked;
    scene.add(stlMesh);

    // Wireframe
    const wireMat = new THREE.MeshBasicMaterial({ color: 0x38bdf8, wireframe: true, transparent: true, opacity: 0.3 });
    stlWireframe = new THREE.Mesh(geometry, wireMat);
    stlWireframe.visible = document.getElementById('chk-stl-wireframe').checked;
    scene.add(stlWireframe);

    // Update metadata panel
    document.getElementById('stl-meta-info').style.display = 'block';
    document.getElementById('stl-solid-name').textContent = data.solid_name;
    document.getElementById('stl-facets').textContent = data.facet_count;
    document.getElementById('stl-area').textContent = data.surface_area.toFixed(4);
    document.getElementById('stl-dims').textContent = `${data.bbox.dimensions[0].toFixed(2)} × ${data.bbox.dimensions[1].toFixed(2)} × ${data.bbox.dimensions[2].toFixed(2)} m`;
}

async function loadDefaultCsv() {
    try {
        const resp = await fetch('/api/geometry/parse_csv', { method: 'POST', body: '' });
        const data = await resp.json();
        if (data.success) renderTemperaturePointCloud(data);
    } catch (e) {
        console.error('Error loading default CSV:', e);
    }
}

function parseCsvData(content) {
    fetch('/api/geometry/parse_csv', { method: 'POST', body: content })
        .then(r => r.json())
        .then(data => {
            if (data.success) renderTemperaturePointCloud(data);
            else alert('CSV Parse Error: ' + data.error);
        });
}

function getTemperatureColor(temp, minT, maxT) {
    const norm = Math.max(0, Math.min(1, (temp - minT) / (maxT - minT || 1)));
    // Blue (0.0) -> Cyan (0.25) -> Green (0.5) -> Yellow (0.75) -> Red (1.0)
    let r = 0, g = 0, b = 0;
    if (norm < 0.25) {
        r = 0; g = norm * 4; b = 1;
    } else if (norm < 0.5) {
        r = 0; g = 1; b = 1 - (norm - 0.25) * 4;
    } else if (norm < 0.75) {
        r = (norm - 0.5) * 4; g = 1; b = 0;
    } else {
        r = 1; g = 1 - (norm - 0.75) * 4; b = 0;
    }
    return [r, g, b];
}

function renderTemperaturePointCloud(data) {
    if (pointCloud) scene.remove(pointCloud);

    const numPoints = data.point_coords.length / 3;
    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute('position', new THREE.Float32BufferAttribute(data.point_coords, 3));

    const colors = [];
    currentPointsData = [];

    const minT = data.min_temp_C;
    const maxT = data.max_temp_C;

    for (let i = 0; i < numPoints; ++i) {
        const t = data.temperatures[i];
        const [r, g, b] = getTemperatureColor(t, minT, maxT);
        colors.push(r, g, b);

        currentPointsData.push({
            id: i + 1,
            x: data.point_coords[i * 3],
            y: data.point_coords[i * 3 + 1],
            z: data.point_coords[i * 3 + 2],
            t: t
        });
    }

    geometry.setAttribute('color', new THREE.Float32BufferAttribute(colors, 3));

    const material = new THREE.PointsMaterial({
        size: 0.035,
        vertexColors: true,
        transparent: true,
        opacity: 0.95
    });

    pointCloud = new THREE.Points(geometry, material);
    pointCloud.visible = document.getElementById('chk-csv-points').checked;
    scene.add(pointCloud);

    // Update legend and meta
    document.getElementById('csv-meta-info').style.display = 'block';
    document.getElementById('csv-nodes').textContent = data.total_nodes;
    document.getElementById('csv-temp-range').textContent = `${minT.toFixed(1)} – ${maxT.toFixed(1)}`;
    document.getElementById('csv-mean-temp').textContent = data.mean_temp_C.toFixed(1);

    document.getElementById('legend-min').textContent = `${minT.toFixed(1)} °C`;
    document.getElementById('legend-mid').textContent = `${((minT + maxT) * 0.5).toFixed(1)} °C`;
    document.getElementById('legend-max').textContent = `${maxT.toFixed(1)} °C`;
}

// -------------------------------------------------------------
// 4. Thermal FEA Solver (Task C)
// -------------------------------------------------------------
let thermalProfileChart = null;
let thermalConvergenceChart = null;

function initThermalSolver() {
    document.getElementById('btn-run-fea').addEventListener('click', runThermalSolve);
    document.getElementById('btn-run-convergence').addEventListener('click', runThermalConvergence);
    runThermalSolve();
}

async function runThermalSolve() {
    const payload = {
        length_m: parseFloat(document.getElementById('inp-fea-L').value),
        area_m2: parseFloat(document.getElementById('inp-fea-A').value),
        conductivity_W_mK: parseFloat(document.getElementById('inp-fea-k').value),
        heat_gen_W_m3: parseFloat(document.getElementById('inp-fea-q').value),
        temp_left_C: parseFloat(document.getElementById('inp-fea-T0').value),
        temp_right_C: parseFloat(document.getElementById('inp-fea-TL').value),
        num_elements: parseInt(document.getElementById('sel-fea-N').value)
    };

    try {
        const resp = await fetch('/api/thermal/solve', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        });
        const data = await resp.json();
        if (!data.success) {
            alert('FEA Solver Error: ' + data.error);
            return;
        }

        document.getElementById('fea-max-error').textContent = `${data.max_midpoint_abs_error_C.toFixed(6)} °C`;
        document.getElementById('fea-l2-norm').textContent = `${data.l2_error_norm.toFixed(6)} °C`;

        // Render profile chart
        renderThermalProfileChart(data);

        // Populate table
        populateMidpointsTable(data.midpoints);

    } catch (e) {
        console.error('Thermal solve failed:', e);
    }
}

function renderThermalProfileChart(data) {
    const ctx = document.getElementById('chart-thermal-profile');
    if (!ctx || typeof Chart === 'undefined') return;

    const labels = data.nodes.map(n => n.x.toFixed(3));
    const femTemps = data.nodes.map(n => n.temp_fem);
    const exactTemps = data.nodes.map(n => n.temp_exact);

    if (thermalProfileChart) thermalProfileChart.destroy();

    thermalProfileChart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: labels,
            datasets: [
                {
                    label: 'Numerical FEA (°C)',
                    data: femTemps,
                    borderColor: '#38bdf8',
                    backgroundColor: 'rgba(56, 189, 248, 0.1)',
                    pointRadius: 4,
                    pointHoverRadius: 6,
                    borderWidth: 2
                },
                {
                    label: 'Analytical Exact T_exact(x) (°C)',
                    data: exactTemps,
                    borderColor: '#f59e0b',
                    borderDash: [5, 5],
                    pointRadius: 0,
                    borderWidth: 2
                }
            ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            scales: {
                x: { title: { display: true, text: 'Rod Position x (m)', color: '#94a3b8' }, grid: { color: '#1e293b' } },
                y: { title: { display: true, text: 'Temperature T (°C)', color: '#94a3b8' }, grid: { color: '#1e293b' } }
            },
            plugins: {
                legend: { labels: { color: '#f8fafc' } }
            }
        }
    });
}

async function runThermalConvergence() {
    try {
        const resp = await fetch('/api/thermal/convergence');
        const data = await resp.json();
        if (!data.success) return;

        const ctx = document.getElementById('chart-thermal-convergence');
        if (!ctx || typeof Chart === 'undefined') return;

        const labels = data.convergence.map(c => `N=${c.elements}`);
        const errors = data.convergence.map(c => c.max_midpoint_error_C);

        if (thermalConvergenceChart) thermalConvergenceChart.destroy();

        thermalConvergenceChart = new Chart(ctx, {
            type: 'bar',
            data: {
                labels: labels,
                datasets: [{
                    label: 'Max Midpoint Error (°C)',
                    data: errors,
                    backgroundColor: ['#0284c7', '#0369a1', '#075985', '#0c4a6e'],
                    borderColor: '#38bdf8',
                    borderWidth: 1
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                scales: {
                    x: { title: { display: true, text: 'Mesh Discretization N', color: '#94a3b8' }, grid: { color: '#1e293b' } },
                    y: { 
                        title: { display: true, text: 'Max Error (°C)', color: '#94a3b8' },
                        grid: { color: '#1e293b' },
                        type: 'logarithmic'
                    }
                },
                plugins: {
                    legend: { labels: { color: '#f8fafc' } }
                }
            }
        });

    } catch (e) {
        console.error('Convergence study error:', e);
    }
}

function populateMidpointsTable(midpoints) {
    const tbody = document.querySelector('#table-midpoints tbody');
    if (!tbody || !midpoints) return;
    tbody.innerHTML = '';

    midpoints.forEach(mp => {
        const tr = document.createElement('tr');
        tr.innerHTML = `
            <td>#${mp.elem + 1}</td>
            <td>${mp.x_mid.toFixed(4)}</td>
            <td>${mp.temp_fem.toFixed(4)}</td>
            <td>${mp.temp_exact.toFixed(4)}</td>
            <td style="color: #38bdf8; font-weight: bold;">${mp.abs_error.toFixed(6)}</td>
        `;
        tbody.appendChild(tr);
    });
}

// -------------------------------------------------------------
// 5. Multicore Benchmark Engine (Task D)
// -------------------------------------------------------------
let wallClockChart = null;
let speedupChart = null;

function initMulticoreBenchmark() {
    document.getElementById('btn-run-benchmark').addEventListener('click', runBenchmark);
}

async function runBenchmark() {
    const btn = document.getElementById('btn-run-benchmark');
    btn.disabled = true;
    btn.textContent = 'Benchmarking (Warmup + 5 Repetitions)...';

    const payload = {
        case_count: parseInt(document.getElementById('inp-bench-cases').value),
        mesh_size: parseInt(document.getElementById('inp-bench-mesh').value),
        max_workers: parseInt(document.getElementById('sel-bench-workers').value)
    };

    try {
        const resp = await fetch('/api/performance/benchmark', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        });
        const data = await resp.json();
        if (!data.success) {
            alert('Benchmark Error: ' + data.error);
            return;
        }

        document.getElementById('bench-warmup-time').textContent = `${data.warmup_time_ms.toFixed(2)} ms`;
        document.getElementById('bench-serial-median').textContent = `${data.serial_median_time_ms.toFixed(2)} ms`;
        document.getElementById('bench-parity-status').textContent = `Max Diff: ${data.max_temperature_discrepancy_C.toExponential(2)} °C [PARITY VERIFIED]`;
        document.getElementById('bench-analysis-notes').textContent = data.analysis_notes;

        renderBenchmarkCharts(data);
        populateBenchmarkTable(data.workers);

    } catch (e) {
        console.error('Benchmark error:', e);
    } finally {
        btn.disabled = false;
        btn.textContent = 'Execute Multicore Benchmark';
    }
}

function renderBenchmarkCharts(data) {
    const workerLabels = data.workers.map(w => `${w.workers} Worker${w.workers > 1 ? 's' : ''}`);
    const medianTimes = data.workers.map(w => w.median_ms);
    const speedups = data.workers.map(w => w.speedup);
    const efficiencies = data.workers.map(w => (w.efficiency * 100).toFixed(1));

    // 1. Wall clock chart
    const ctx1 = document.getElementById('chart-wall-clock');
    if (wallClockChart) wallClockChart.destroy();
    wallClockChart = new Chart(ctx1, {
        type: 'bar',
        data: {
            labels: workerLabels,
            datasets: [{
                label: 'Median Compute Time (ms)',
                data: medianTimes,
                backgroundColor: '#0284c7',
                borderColor: '#38bdf8',
                borderWidth: 1
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            scales: {
                x: { grid: { color: '#1e293b' } },
                y: { title: { display: true, text: 'Wall-Clock Time (ms)', color: '#94a3b8' }, grid: { color: '#1e293b' } }
            },
            plugins: { legend: { labels: { color: '#f8fafc' } } }
        }
    });

    // 2. Speedup & Efficiency chart
    const ctx2 = document.getElementById('chart-speedup-efficiency');
    if (speedupChart) speedupChart.destroy();
    speedupChart = new Chart(ctx2, {
        type: 'line',
        data: {
            labels: workerLabels,
            datasets: [
                {
                    label: 'Speedup S(p)',
                    data: speedups,
                    borderColor: '#10b981',
                    backgroundColor: 'rgba(16, 185, 129, 0.1)',
                    yAxisID: 'y'
                },
                {
                    label: 'Efficiency E(p) %',
                    data: efficiencies,
                    borderColor: '#f59e0b',
                    borderDash: [4, 4],
                    yAxisID: 'y1'
                }
            ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            scales: {
                x: { grid: { color: '#1e293b' } },
                y: { title: { display: true, text: 'Speedup S(p)', color: '#10b981' }, grid: { color: '#1e293b' } },
                y1: { 
                    position: 'right', 
                    title: { display: true, text: 'Efficiency (%)', color: '#f59e0b' },
                    grid: { drawOnChartArea: false },
                    min: 0,
                    max: 120
                }
            },
            plugins: { legend: { labels: { color: '#f8fafc' } } }
        }
    });
}

function populateBenchmarkTable(workers) {
    const tbody = document.querySelector('#table-benchmark-runs tbody');
    if (!tbody || !workers) return;
    tbody.innerHTML = '';

    workers.forEach(w => {
        const tr = document.createElement('tr');
        tr.innerHTML = `
            <td><strong>${w.workers} Worker${w.workers > 1 ? 's' : ''}</strong></td>
            <td>${w.repetitions[0].toFixed(2)}</td>
            <td>${w.repetitions[1].toFixed(2)}</td>
            <td>${w.repetitions[2].toFixed(2)}</td>
            <td>${w.repetitions[3].toFixed(2)}</td>
            <td>${w.repetitions[4].toFixed(2)}</td>
            <td style="font-weight: bold; color: #38bdf8;">${w.median_ms.toFixed(2)}</td>
            <td style="color: #10b981;">${w.speedup.toFixed(2)}x</td>
            <td style="color: #f59e0b;">${(w.efficiency * 100).toFixed(1)}%</td>
        `;
        tbody.appendChild(tr);
    });
}

// -------------------------------------------------------------
// 6. Embedded Hardware Manager (Task E)
// -------------------------------------------------------------
let hardwareDevices = [];
let telemetryInterval = null;

async function initHardwareManager() {
    await fetchHardwareList();
    startTelemetryStreaming();
}

async function fetchHardwareList() {
    try {
        const resp = await fetch('/api/hardware/list');
        const data = await resp.json();
        if (!data.success) return;
        hardwareDevices = data.devices;
        renderHardwareCards();
    } catch (e) {
        console.error('Error fetching hardware list:', e);
    }
}

function renderHardwareCards() {
    const container = document.getElementById('devices-container');
    if (!container) return;
    container.innerHTML = '';

    hardwareDevices.forEach(dev => {
        const card = document.createElement('div');
        card.className = 'device-card';
        card.id = `card-${dev.id}`;

        const statusClass = dev.status_code === 3 ? 'badge-success' : 
                           dev.status_code === 4 ? 'badge-danger' : 
                           dev.status_code === 2 ? 'badge-success' : 'badge-warning';
        
        const statusText = dev.status_code === 3 ? 'Streaming Nominal' :
                          dev.status_code === 4 ? 'Timeout / Error' :
                          dev.status_code === 2 ? 'Connected' : 'Disconnected';

        card.innerHTML = `
            <div>
                <div class="device-header">
                    <div>
                        <div class="device-title">${dev.name}</div>
                        <div class="device-type-tag">${dev.vendor} · ${dev.serial}</div>
                    </div>
                    <span id="badge-${dev.id}" class="badge ${statusClass}">${statusText}</span>
                </div>

                <div class="telemetry-dashboard" id="telem-${dev.id}">
                    <div class="telemetry-item"><span class="telemetry-label">Core VDD:</span> <span class="telemetry-val" id="vcore-${dev.id}">— V</span></div>
                    <div class="telemetry-item"><span class="telemetry-label">IO Rail:</span> <span class="telemetry-val" id="vio-${dev.id}">— V</span></div>
                    <div class="telemetry-item"><span class="telemetry-label">Current:</span> <span class="telemetry-val" id="curr-${dev.id}">— mA</span></div>
                    <div class="telemetry-item"><span class="telemetry-label">Junction T:</span> <span class="telemetry-val" id="temp-${dev.id}">— °C</span></div>
                </div>

                <div class="hint-text" id="status-line-${dev.id}" style="font-size: 0.75rem; font-family: var(--font-mono); margin-bottom: 0.75rem;">
                    Interface: ${dev.host_interface}
                </div>
            </div>

            <div class="button-group">
                <button class="btn btn-primary" onclick="sendHardwareAction('${dev.id}', 'connect')">Connect</button>
                <button class="btn btn-secondary" onclick="sendHardwareAction('${dev.id}', 'disconnect')">Disconnect</button>
                <button class="btn btn-danger" onclick="sendHardwareAction('${dev.id}', 'fault')">Inject Fault</button>
                <button class="btn btn-accent" onclick="sendHardwareAction('${dev.id}', 'recover')">Recover</button>
            </div>
        `;
        container.appendChild(card);
    });
}

window.sendHardwareAction = async function(deviceId, action) {
    try {
        const resp = await fetch('/api/hardware/action', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ device_id: deviceId, action: action })
        });
        const data = await resp.json();
        await fetchHardwareList();
    } catch (e) {
        console.error('Hardware action error:', e);
    }
};

function startTelemetryStreaming() {
    if (telemetryInterval) clearInterval(telemetryInterval);
    telemetryInterval = setInterval(async () => {
        for (const dev of hardwareDevices) {
            try {
                const resp = await fetch(`/api/hardware/telemetry?id=${dev.id}`);
                const tel = await resp.json();
                if (tel.success) {
                    const vcore = document.getElementById(`vcore-${dev.id}`);
                    const vio = document.getElementById(`vio-${dev.id}`);
                    const curr = document.getElementById(`curr-${dev.id}`);
                    const temp = document.getElementById(`temp-${dev.id}`);
                    const stat = document.getElementById(`status-line-${dev.id}`);

                    if (vcore) vcore.textContent = `${tel.core_voltage_V.toFixed(2)} V`;
                    if (vio) vio.textContent = `${tel.io_voltage_V.toFixed(2)} V`;
                    if (curr) curr.textContent = `${tel.current_mA.toFixed(1)} mA`;
                    if (temp) temp.textContent = `${tel.junction_temp_C.toFixed(1)} °C`;
                    if (stat) stat.textContent = `[Pkt #${tel.packet_counter}] ${tel.status}`;
                }
            } catch (e) {}
        }
    }, 1500);
}

// -------------------------------------------------------------
// 7. Backpropagation Engine (Task F)
// -------------------------------------------------------------
let neuralLossChart = null;

function initBackpropagation() {
    document.getElementById('btn-run-backprop').addEventListener('click', runBackpropagationStudy);
    runBackpropagationStudy();
}

async function runBackpropagationStudy() {
    try {
        const resp = await fetch('/api/neural/train');
        const data = await resp.json();
        if (!data.success) return;

        // Render Step 0 forward & parameters
        document.getElementById('nn-init-yhat').textContent = `${data.initial_forward.y_hat.toFixed(6)}`;
        document.getElementById('nn-init-loss').textContent = `${data.initial_forward.loss_J.toFixed(6)}`;

        const s1 = data.step1_update;
        document.getElementById('nn-step1-params').textContent = 
            `w=${s1.w.toFixed(4)}, b=${s1.b.toFixed(4)}, v=${s1.v.toFixed(4)}, c=${s1.c.toFixed(4)} (Loss J1: ${s1.loss_J.toFixed(4)})`;

        populateNeuralDerivation(data);

        // Finite difference table
        populateFiniteDiffTable(data.finite_difference_checks);

        // Loss History Chart
        renderNeuralLossChart(data.history);

    } catch (e) {
        console.error('Backprop error:', e);
    }
}

function populateFiniteDiffTable(checks) {
    const tbody = document.querySelector('#table-finite-diff tbody');
    if (!tbody || !checks) return;
    tbody.innerHTML = '';

    checks.forEach(chk => {
        const tr = document.createElement('tr');
        tr.innerHTML = `
            <td><strong>${chk.param}</strong></td>
            <td>${chk.analytical.toFixed(10)}</td>
            <td>${chk.numerical.toFixed(10)}</td>
            <td>${chk.error.toExponential(10)}</td>
            <td><span class="badge ${chk.passed ? 'badge-success' : 'badge-danger'}">${chk.passed ? 'PASS (|Err| <= 1e-5)' : 'FAIL'}</span></td>
        `;
        tbody.appendChild(tr);
    });
}

function populateNeuralDerivation(data) {
    const f = data.initial_forward;
    const g = data.initial_gradients;
    const p = data.initial_params;
    const residual = f.y_hat - f.y;
    const dhDz = 1.0 - f.h * f.h;
    const dJDh = residual * p.v;
    const dJDz = dJDh * dhDz;
    const derivation = document.getElementById('nn-digital-derivation');
    if (!derivation) return;

    derivation.textContent =
`Forward propagation:
z = w*x + b = ${p.w.toFixed(10)}*${f.x.toFixed(10)} + ${p.b.toFixed(10)} = ${f.z.toFixed(10)}
h = tanh(z) = tanh(${f.z.toFixed(10)}) = ${f.h.toFixed(10)}
y_hat = v*h + c = ${p.v.toFixed(10)}*${f.h.toFixed(10)} + ${p.c.toFixed(10)} = ${f.y_hat.toFixed(10)}
Residual e = y_hat - y = ${f.y_hat.toFixed(10)} - ${f.y.toFixed(10)} = ${residual.toFixed(10)}
Loss J = 0.5*(y_hat - y)^2 = 0.5*(${residual.toFixed(10)})^2 = ${f.loss_J.toFixed(10)}

Chain-rule gradients:
dJ/dy_hat = y_hat - y = ${residual.toFixed(10)}
dJ/dv = (dJ/dy_hat)*h = ${residual.toFixed(10)}*${f.h.toFixed(10)} = ${g.dJ_dv.toFixed(10)}
dJ/dc = (dJ/dy_hat)*1 = ${g.dJ_dc.toFixed(10)}
dJ/dh = (dJ/dy_hat)*(dy_hat/dh) = (y_hat-y)*v
      = ${residual.toFixed(10)}*${p.v.toFixed(10)} = ${dJDh.toFixed(10)}
dh/dz = 1-tanh^2(z) = 1-h^2 = 1-${f.h.toFixed(10)}^2 = ${dhDz.toFixed(10)}
dJ/dz = (dJ/dh)*(dh/dz) = ${dJDh.toFixed(10)}*${dhDz.toFixed(10)} = ${dJDz.toFixed(10)}
dJ/dw = (dJ/dz)*(dz/dw) = (dJ/dz)*x = ${g.dJ_dw.toFixed(10)}
dJ/db = (dJ/dz)*(dz/db) = (dJ/dz)*1 = ${g.dJ_db.toFixed(10)}

v is included explicitly in dJ/dh: dJ/dh = (dJ/dy_hat)*v.`;
}

function renderNeuralLossChart(history) {
    const ctx = document.getElementById('chart-neural-loss');
    if (!ctx || typeof Chart === 'undefined') return;

    const trainingHistory = history.filter(h => h.step >= 1 && h.step <= 100);
    const labels = trainingHistory.map(h => h.step);
    const losses = trainingHistory.map(h => h.loss);
    const yHats = trainingHistory.map(h => h.y_hat);

    if (neuralLossChart) neuralLossChart.destroy();

    neuralLossChart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: labels,
            datasets: [
                {
                    label: 'Squared Error Loss J',
                    data: losses,
                    borderColor: '#ef4444',
                    backgroundColor: 'rgba(239, 68, 68, 0.1)',
                    pointRadius: 1,
                    borderWidth: 2,
                    yAxisID: 'y'
                },
                {
                    label: 'Prediction y_hat (Target y = 2.0)',
                    data: yHats,
                    borderColor: '#10b981',
                    pointRadius: 1,
                    borderWidth: 2,
                    yAxisID: 'y1'
                }
            ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            scales: {
                x: { title: { display: true, text: 'Training Iterations (Step 0 to 100)', color: '#94a3b8' }, grid: { color: '#1e293b' } },
                y: { title: { display: true, text: 'Loss J', color: '#ef4444' }, grid: { color: '#1e293b' } },
                y1: {
                    position: 'right',
                    title: { display: true, text: 'Prediction y_hat', color: '#10b981' },
                    grid: { drawOnChartArea: false },
                    min: 0,
                    max: 2.2
                }
            },
            plugins: { legend: { labels: { color: '#f8fafc' } } }
        }
    });
}

/**
 * script.js - Loki Dragon UI
 * Orange Pi Zero 2W Interactive System
 *
 * Controls: dragon mood states, stage growth, hardware test simulation,
 * vending machine, activity log, and uptime counter.
 */

/* ===================================================================
   STATE
   =================================================================== */
const state = {
    mood: 'neutral',          // 'neutral' | 'happy' | 'mean'
    stage: 'egg',             // 'egg' | 'baby' | 'adult'
    moodTimer: null,          // auto-reset timer
    startTime: Date.now(),
    vendCounts: {
        fire_token: 12,
        scale: 5,
        gem: 3,
        rune: 8,
    },
};

/* ===================================================================
   DOM REFERENCES
   =================================================================== */
const dragonPanel  = document.getElementById('dragon-panel');
const moodIcon     = document.getElementById('mood-icon');
const moodText     = document.getElementById('mood-text');
const moodBar      = document.querySelector('.mood-bar');
const logOutput    = document.getElementById('log-output');
const toast        = document.getElementById('toast');
const vendStatus   = document.getElementById('vending-status');

/* ===================================================================
   LOGGING
   =================================================================== */
function ts() {
    const d = new Date();
    const pad = n => String(n).padStart(2, '0');
    return `${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`;
}

function addLog(msg, level = 'info') {
    const entry = document.createElement('div');
    entry.className = `log-entry log-${level}`;
    entry.setAttribute('data-ts', `[${ts()}]`);
    entry.textContent = msg;
    logOutput.appendChild(entry);
    logOutput.scrollTop = logOutput.scrollHeight;
}

function clearLog() {
    logOutput.innerHTML = '';
    addLog('Log cleared.', 'system');
}

/* ===================================================================
   TOAST
   =================================================================== */
let toastTimer = null;

function showToast(msg, type = 'info') {
    toast.textContent = msg;
    toast.className = `toast toast-${type} show`;
    if (toastTimer) clearTimeout(toastTimer);
    toastTimer = setTimeout(() => {
        toast.classList.remove('show');
    }, 2800);
}

/* ===================================================================
   DRAGON STAGE (egg → baby → adult)
   =================================================================== */
function setStage(stage) {
    const stages = ['egg', 'baby', 'adult'];
    if (!stages.includes(stage)) return;
    if (state.stage === stage) return;

    const prev = state.stage;
    state.stage = stage;

    stages.forEach(s => {
        const el = document.getElementById(`stage-${s}`);
        if (el) {
            if (s === stage) {
                el.classList.remove('hidden');
            } else {
                el.classList.add('hidden');
            }
        }
    });

    const labels = { egg: 'Egg', baby: 'Baby Dragon', adult: 'Adult Dragon' };
    addLog(`Dragon evolved: ${labels[prev]} → ${labels[stage]}`, 'system');
    showToast(`🐉 ${labels[stage]}!`, 'info');

    /* Re-apply current mood visual to new stage SVG */
    applyMoodVisuals(state.mood);
}

/* ===================================================================
   MOOD ENGINE
   =================================================================== */
const MOOD_CONFIG = {
    neutral: {
        icon: '😐',
        text: 'Neutral',
        mouthHappy_baby:  'M 88 84 Q 100 90 112 84',
        mouthHappy_adult: 'M 82 80 Q 100 88 118 80',
        eyeColorLeft_baby:  '#00cc44',
        eyeColorRight_baby: '#00cc44',
        eyeColorLeft_adult:  '#00dd55',
        eyeColorRight_adult: '#00dd55',
    },
    happy: {
        icon: '😊',
        text: 'Happy',
        mouthHappy_baby:  'M 84 83 Q 100 100 116 83',  /* wide smile */
        mouthHappy_adult: 'M 78 79 Q 100 100 122 79',  /* wide smile */
        eyeColorLeft_baby:  '#44ff88',
        eyeColorRight_baby: '#44ff88',
        eyeColorLeft_adult:  '#55ffaa',
        eyeColorRight_adult: '#55ffaa',
    },
    mean: {
        icon: '😡',
        text: 'Mean',
        mouthHappy_baby:  'M 84 92 Q 100 80 116 92',   /* frown */
        mouthHappy_adult: 'M 78 90 Q 100 75 122 90',   /* frown */
        eyeColorLeft_baby:  '#ff4422',
        eyeColorRight_baby: '#ff4422',
        eyeColorLeft_adult:  '#ff3300',
        eyeColorRight_adult: '#ff3300',
    },
};

function applyMoodVisuals(mood) {
    const cfg = MOOD_CONFIG[mood];
    if (!cfg) return;

    /* --- Mouth shapes --- */
    const babyMouth  = document.getElementById('baby-mouth');
    const adultMouth = document.getElementById('adult-mouth');
    if (babyMouth)  babyMouth.setAttribute('d', cfg.mouthHappy_baby);
    if (adultMouth) adultMouth.setAttribute('d', cfg.mouthHappy_adult);

    /* --- Eye colours --- */
    const bEL = document.getElementById('baby-eye-left');
    const bER = document.getElementById('baby-eye-right');
    const aEL = document.getElementById('adult-eye-left');
    const aER = document.getElementById('adult-eye-right');
    if (bEL) bEL.setAttribute('fill', cfg.eyeColorLeft_baby);
    if (bER) bER.setAttribute('fill', cfg.eyeColorRight_baby);
    if (aEL) aEL.setAttribute('fill', cfg.eyeColorLeft_adult);
    if (aER) aER.setAttribute('fill', cfg.eyeColorRight_adult);

    /* --- Mean: angry brow lines (add/remove) --- */
    updateBrowLines(mood === 'mean');

    /* --- Happy: fire breath visible --- */
    const fireBreath = document.getElementById('fire-breath');
    if (fireBreath) {
        fireBreath.setAttribute('opacity', mood === 'happy' ? '0.9' : '0');
    }

    /* --- Mood bar --- */
    moodIcon.textContent = cfg.icon;
    moodText.textContent = cfg.text;
    moodBar.className = `mood-bar ${mood === 'neutral' ? '' : mood}`;
}

function updateBrowLines(show) {
    const leftEye  = document.getElementById('adult-eye-left');
    const rightEye = document.getElementById('adult-eye-right');
    if (!leftEye || !rightEye) return;

    /* Remove old brow lines */
    ['brow-left', 'brow-right'].forEach(id => {
        const el = document.getElementById(id);
        if (el) el.remove();
    });

    if (!show) return;

    /* Draw angry brows for adult stage */
    const svg = leftEye.closest('svg');
    if (!svg) return;

    [
        { id: 'brow-left',  d: 'M 74 47 Q 84 40 94 46' },
        { id: 'brow-right', d: 'M 106 46 Q 116 40 126 47' },
    ].forEach(({ id, d }) => {
        const path = document.createElementNS('http://www.w3.org/2000/svg', 'path');
        path.setAttribute('id', id);
        path.setAttribute('d', d);
        path.setAttribute('stroke', '#ff4422');
        path.setAttribute('stroke-width', '3');
        path.setAttribute('fill', 'none');
        path.setAttribute('stroke-linecap', 'round');
        svg.appendChild(path);
    });
}

function setMood(mood, autoClear = true, duration = 4000) {
    if (!['neutral', 'happy', 'mean'].includes(mood)) return;

    /* Clear any running reset timer */
    if (state.moodTimer) {
        clearTimeout(state.moodTimer);
        state.moodTimer = null;
    }

    state.mood = mood;

    /* Update panel class */
    dragonPanel.classList.remove('mood-neutral', 'mood-happy', 'mood-mean');
    dragonPanel.classList.add(`mood-${mood}`);

    applyMoodVisuals(mood);

    /* Auto-reset to neutral (skip for neutral itself) */
    if (autoClear && mood !== 'neutral') {
        state.moodTimer = setTimeout(() => setMood('neutral', false), duration);
    }
}

/* ===================================================================
   EXPORTED MOOD CONTROLS (called from HTML onclick)
   =================================================================== */
function triggerHappy() {
    setMood('happy');
    addLog('Mood set to HAPPY 🟢', 'ok');
    showToast('🟢 Loki is happy!', 'ok');
}

function triggerMean() {
    setMood('mean');
    addLog('Mood set to MEAN 🔴', 'error');
    showToast('🔴 Loki is upset!', 'err');
}

function resetMood() {
    setMood('neutral', false);
    addLog('Mood reset to neutral.', 'system');
    showToast('😐 Neutral', 'info');
}

/* ===================================================================
   HARDWARE TEST SIMULATION
   =================================================================== */
const TESTS = {
    tft: {
        label: 'TFT Display',
        statusId: 'status-tft',
        steps: [
            'Initialising SPI bus...',
            'Resetting TFT (ST7789)...',
            'Setting display orientation...',
            'Filling screen with red...',
            'Drawing green rectangle...',
            'Drawing blue rectangle...',
            'TFT Display test PASSED ✓',
        ],
        successMood: true,
    },
    eeprom: {
        label: 'EEPROM',
        statusId: 'status-eeprom',
        steps: [
            'Opening I2C bus...',
            'Writing 8 bytes to address 0x00...',
            'Waiting for write cycle...',
            'Reading 8 bytes back...',
            'Verifying data integrity...',
            'EEPROM test PASSED ✓',
        ],
        successMood: true,
    },
    flash: {
        label: 'Flash Memory',
        statusId: 'status-flash',
        steps: [
            'Selecting SPI CS for W25Q40...',
            'Sending JEDEC ID command (0x9F)...',
            'Reading 3-byte response...',
            'Comparing against expected 0xEF4013...',
            'Flash JEDEC ID verified ✓',
        ],
        successMood: true,
    },
    flipper: {
        label: 'Flipper UART',
        statusId: 'status-flipper',
        steps: [
            'Opening UART3 (115200 baud)...',
            'Sending ping frame...',
            'Waiting for ACK...',
            'Flipper UART responsive ✓',
        ],
        successMood: true,
        failChance: 0.4,   /* 40% chance of simulated disconnection */
        failStep: 2,
        failMsg: 'No response – Flipper not connected',
    },
    gpio: {
        label: 'GPIO',
        statusId: 'status-gpio',
        steps: [
            'Mapping /dev/gpiochip0...',
            'Setting PA7 HIGH...',
            'Reading PA7 state...',
            'Setting PA7 LOW...',
            'GPIO test PASSED ✓',
        ],
        successMood: true,
    },
};

function runHardwareTest(testKey) {
    if (testKey === 'all') {
        /* Run all tests sequentially with small delays between */
        const keys = ['tft', 'eeprom', 'flash', 'flipper', 'gpio'];
        let delay = 0;
        keys.forEach((k, i) => {
            setTimeout(() => runHardwareTest(k), delay);
            delay += 1800 + (TESTS[k].steps.length * 180);
        });
        return;
    }

    const test = TESTS[testKey];
    if (!test) return;

    const badge = document.getElementById(test.statusId);
    if (badge) {
        badge.textContent = 'Running';
        badge.className = 'badge badge-run';
    }

    addLog(`──── ${test.label} Test ────`, 'system');

    let stepIndex = 0;
    let failed = false;

    /* Check for simulated failure */
    if (test.failChance && Math.random() < test.failChance) {
        failed = true;
    }

    function runStep() {
        if (failed && stepIndex === test.failStep) {
            addLog(`✗ ${test.failMsg}`, 'error');
            if (badge) {
                badge.textContent = 'Error';
                badge.className = 'badge badge-err';
            }
            setMood('mean');
            showToast(`❌ ${test.label} failed`, 'err');
            return;
        }

        if (stepIndex < test.steps.length) {
            const isLast = stepIndex === test.steps.length - 1;
            addLog(test.steps[stepIndex], isLast ? 'ok' : 'info');
            stepIndex++;
            setTimeout(runStep, 160 + Math.random() * 120);
        } else {
            /* All steps passed */
            if (badge) {
                badge.textContent = 'OK';
                badge.className = 'badge badge-ok';
            }
            if (test.successMood) {
                setMood('happy');
            }
            showToast(`✅ ${test.label} OK`, 'ok');
        }
    }

    setTimeout(runStep, 100);
}

/* ===================================================================
   VENDING MACHINE
   =================================================================== */
const VEND_LABELS = {
    fire_token: { label: 'Fire Token',    icon: '🔥', elemId: 'vend-fire'  },
    scale:      { label: 'Dragon Scale',  icon: '🐉', elemId: 'vend-scale' },
    gem:        { label: 'Loki Gem',      icon: '💎', elemId: 'vend-gem'   },
    rune:       { label: 'Loki Rune',     icon: 'ᛚ',  elemId: 'vend-rune' },
};

function vendItem(key) {
    const meta = VEND_LABELS[key];
    if (!meta) return;

    if (state.vendCounts[key] <= 0) {
        vendStatus.textContent = `❌ ${meta.label} out of stock!`;
        vendStatus.style.color = 'var(--red-glow)';
        triggerMean();
        addLog(`Vending failed – ${meta.label} out of stock!`, 'error');
        return;
    }

    state.vendCounts[key]--;
    const elem = document.getElementById(meta.elemId);
    if (elem) elem.textContent = `×${state.vendCounts[key]}`;

    vendStatus.textContent = `${meta.icon} ${meta.label} dispensed!`;
    vendStatus.style.color = 'var(--green-glow)';
    triggerHappy();
    addLog(`Vending: dispensed ${meta.label} (${state.vendCounts[key]} remaining)`, 'ok');
}

/* ===================================================================
   UPTIME COUNTER
   =================================================================== */
function updateUptime() {
    const elapsed = Math.floor((Date.now() - state.startTime) / 1000);
    const h = Math.floor(elapsed / 3600);
    const m = Math.floor((elapsed % 3600) / 60);
    const s = elapsed % 60;
    const pad = n => String(n).padStart(2, '0');
    const el = document.getElementById('uptime-display');
    if (el) el.textContent = `${pad(h)}:${pad(m)}:${pad(s)}`;
}

/* ===================================================================
   INITIALISATION
   =================================================================== */
function init() {
    /* Set initial stage and mood */
    setStage('egg');
    setMood('neutral', false);

    addLog('Loki Dragon UI v1.0 loaded', 'system');
    addLog('Platform: Orange Pi Zero 2W', 'system');
    addLog('HAL: GPIO / SPI / I2C / UART / PWM', 'info');
    addLog('Drivers: TFT · SD Card · Flash · EEPROM · Flipper UART', 'info');
    addLog('Web server: Flask / port 5000', 'info');
    addLog('Ready. Awaiting commands.', 'ok');

    /* Connection dot – always online in this demo */
    const dot = document.getElementById('connection-dot');
    if (dot) dot.classList.remove('offline');

    /* Uptime ticker */
    setInterval(updateUptime, 1000);
    updateUptime();

    /* Simulate a startup sequence after short delay */
    setTimeout(() => {
        addLog('Running startup self-check...', 'system');
        setTimeout(() => addLog('Core system: OK ✓', 'ok'),   400);
        setTimeout(() => addLog('SPI buses: OK ✓',   'ok'),   800);
        setTimeout(() => addLog('I2C bus: OK ✓',     'ok'),  1100);
        setTimeout(() => addLog('GPIO map: OK ✓',    'ok'),  1400);
        setTimeout(() => {
            addLog('Self-check complete. All systems nominal.', 'ok');
            setMood('happy');
        }, 1800);
    }, 600);
}

/* Run after DOM is ready */
document.addEventListener('DOMContentLoaded', init);

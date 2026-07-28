const defaultConfig = {
  schemaVersion: 1,
  deviceName: "Mynah eInq",
  board: "x3",
  profile: "parent",
  ageBand: "child",
  gatewayUrl: "https://mynah.castalia.institute",
  calendarId: "ca7a560e76044c59bbb72a70b98a21a774b99c2f5195eb7357ecd1a1cdf74344@group.calendar.google.com",
  features: { calendar: true, astrology: true, fortune: true, cards: true },
  rooms: [],
  roomMinimumLead: 5
};

const form = document.querySelector("#config-form");
const saveState = document.querySelector("#save-state");
const roomList = document.querySelector("#room-list");
const roomTemplate = document.querySelector("#room-template");
const confidence = document.querySelector("#room-confidence");
let config = structuredClone(defaultConfig);
let canWrite = true;

function setStatus(message, isError = false) {
  saveState.textContent = message;
  saveState.classList.toggle("error", isError);
}

function showPanel(name) {
  document.querySelectorAll(".panel").forEach((panel) => panel.classList.toggle("active", panel.dataset.panel === name));
  document.querySelectorAll(".step").forEach((step) => step.classList.toggle("active", step.dataset.panel === name));
}

function updateProfileUi() {
  const profile = form.elements.profile.value;
  document.querySelector("#age-field").hidden = profile !== "kid";
}

function addRoom(room = { name: "", beaconId: "", calibrationOffset: 0 }) {
  const fragment = roomTemplate.content.cloneNode(true);
  const row = fragment.querySelector(".room-row");
  row.querySelector("[data-room-name]").value = room.name || "";
  row.querySelector("[data-beacon-id]").value = room.beaconId || "";
  row.querySelector(".remove-room").addEventListener("click", () => row.remove());
  roomList.append(row);
}

function render(next) {
  config = { ...defaultConfig, ...next, features: { ...defaultConfig.features, ...(next.features || {}) } };
  form.elements.deviceName.value = config.deviceName;
  form.elements.board.value = config.board;
  form.elements.profile.value = config.profile;
  form.elements.ageBand.value = config.ageBand;
  form.elements.gatewayUrl.value = config.gatewayUrl;
  form.elements.calendarId.value = config.calendarId || "";
  form.elements.calendarEnabled.checked = config.features.calendar;
  form.elements.astrologyEnabled.checked = config.features.astrology;
  form.elements.fortuneEnabled.checked = config.features.fortune;
  form.elements.cardsEnabled.checked = config.features.cards;
  confidence.value = config.roomMinimumLead;
  document.querySelector("#room-confidence-value").value = `${confidence.value} dB`;
  roomList.replaceChildren();
  (config.rooms || []).forEach(addRoom);
  if (!config.rooms?.length) addRoom();
  updateProfileUi();
}

function collect() {
  const rooms = [...roomList.querySelectorAll(".room-row")].map((row) => ({
    name: row.querySelector("[data-room-name]").value.trim(),
    beaconId: row.querySelector("[data-beacon-id]").value.trim(),
    calibrationOffset: 0
  })).filter((room) => room.name && room.beaconId);
  return {
    schemaVersion: 1,
    deviceName: form.elements.deviceName.value.trim(),
    board: form.elements.board.value,
    profile: form.elements.profile.value,
    ageBand: form.elements.ageBand.value,
    gatewayUrl: form.elements.gatewayUrl.value.trim(),
    calendarId: form.elements.calendarId.value.trim(),
    features: {
      calendar: form.elements.calendarEnabled.checked,
      astrology: form.elements.astrologyEnabled.checked,
      fortune: form.elements.fortuneEnabled.checked,
      cards: form.elements.cardsEnabled.checked
    },
    rooms,
    roomMinimumLead: Number(confidence.value)
  };
}

async function loadConfig() {
  setStatus("Loading settings…");
  try {
    const response = await fetch("/api/v1/config", { headers: { Accept: "application/json" } });
    if (!response.ok) throw new Error(`Device returned ${response.status}`);
    render(await response.json());
    document.querySelector("#device-status").textContent = "Connected";
    setStatus("Settings loaded from your eInq.");
  } catch (error) {
    render(defaultConfig);
    document.querySelector("#device-status").textContent = "Preview";
    setStatus("Device API unavailable; showing a local preview.", true);
  }
}

async function loadDeviceStatus() {
  try {
    const response = await fetch("/api/v1/device", { headers: { Accept: "application/json" } });
    if (!response.ok) return;
    const status = await response.json();
    canWrite = status.canWrite !== false;
    document.querySelector("#network-name").textContent = status.network || "Not configured";
    document.querySelector("#save-config").disabled = !canWrite;
    document.querySelector("#change-wifi").disabled = !canWrite;
    if (!canWrite) {
      setStatus("Read-only on the home network. Long-press the Mynah setup button to make changes.");
    }
  } catch {
    // Static preview and older firmware do not expose device status.
  }
}

form.addEventListener("submit", async (event) => {
  event.preventDefault();
  if (!canWrite) {
    setStatus("Open physical setup mode on the Mynah before saving.", true);
    return;
  }
  const next = collect();
  setStatus("Saving settings…");
  try {
    const response = await fetch("/api/v1/config", {
      method: "PUT",
      headers: { "Content-Type": "application/json", Accept: "application/json" },
      body: JSON.stringify(next)
    });
    if (!response.ok) throw new Error(await response.text());
    render(await response.json());
    setStatus("Settings saved.");
  } catch (error) {
    setStatus(`Could not save: ${error.message}`, true);
  }
});

document.querySelectorAll(".step").forEach((step) => step.addEventListener("click", () => showPanel(step.dataset.panel)));
document.querySelectorAll("[name=profile]").forEach((input) => input.addEventListener("change", updateProfileUi));
document.querySelector("#add-room").addEventListener("click", () => addRoom());
document.querySelector("#refresh-button").addEventListener("click", loadConfig);
confidence.addEventListener("input", () => { document.querySelector("#room-confidence-value").value = `${confidence.value} dB`; });

document.querySelector("#change-wifi").addEventListener("click", () => {
  const fields = document.querySelector("#wifi-fields");
  fields.hidden = !fields.hidden;
});
document.querySelector("#save-wifi").addEventListener("click", async () => {
  if (!canWrite) {
    setStatus("Open physical setup mode on the Mynah before changing WiFi.", true);
    return;
  }
  const body = new URLSearchParams({
    primarySsid: document.querySelector("#wifi-primary-ssid").value,
    primaryPassword: document.querySelector("#wifi-primary-password").value,
    backupSsid: document.querySelector("#wifi-backup-ssid").value,
    backupPassword: document.querySelector("#wifi-backup-password").value
  });
  const response = await fetch("/wifi", { method: "POST", headers: { "Content-Type": "application/x-www-form-urlencoded" }, body });
  if (response.ok) {
    document.querySelector("#network-name").textContent = document.querySelector("#wifi-primary-ssid").value;
    setStatus("WiFi saved. Reconnect to the home network.");
  } else {
    setStatus("WiFi could not be saved.", true);
  }
});

document.querySelectorAll("[data-connect]").forEach((button) => button.addEventListener("click", () => {
  setStatus(`${button.dataset.connect} connection opens after this device is paired with Castalia.`);
}));

if ("serviceWorker" in navigator && window.isSecureContext) {
  navigator.serviceWorker.register("/sw.js").catch(() => {});
}

async function initialize() {
  await loadConfig();
  await loadDeviceStatus();
}

initialize();

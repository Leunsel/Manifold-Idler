"use strict";
(() => {
const token = new URLSearchParams(location.search).get("token") || "";
const $ = (id) => document.getElementById(id);
const pollMilliseconds = 2000;
const retryMilliseconds = 5000;
const statsMilliseconds = 30000;
const libraryStep = 120;
const svgSpace = "http://www.w3.org/2000/svg";

class TokenError extends Error {}

let timer = 0;
let refreshing = false;
let again = false;
let failing = false;
let locked = false;
let active = false;
let page = "idling";
let statusRows = [];
let editing = null;
let queueEntries = [];
let listSignature = "";
let listRows = [];
let toastTimer = 0;
let library = [];
let libraryShown = 0;
let libraryNumber = 0;
let librarySearchTimer = 0;
let libraryLoaded = "";
let selected = "";
let detailNumber = 0;
let panelNumber = 0;
let statsTab = "overview";
let statsAt = 0;
let historyDay = "";
let logRows = [];
let settingsRows = [];
const queueItems = new Map();

const icons = {
  idling: ["X......", "XXX....", "XXXXX..", "XXXXXXX", "XXXXX..", "XXX....", "X......"],
  library: ["XXX.XXX", "XXX.XXX", "XXX.XXX", ".......", "XXX.XXX", "XXX.XXX", "XXX.XXX"],
  statistics: ["......X", "......X", "...X..X", "...X..X", "X..X..X", "X..X..X", "XXXXXXX"],
  log: ["X......", ".X.....", "..X....", ".X.....", "X......", ".......", "...XXXX"],
  settings: ["...X...", ".X.X.X.", "..XXX..", "XXX.XXX", "..XXX..", ".X.X.X.", "...X..."],
  up: ["...X...", "..XXX..", ".XXXXX.", "XXXXXXX", "...X...", "...X...", "...X..."],
  down: ["...X...", "...X...", "...X...", "XXXXXXX", ".XXXXX.", "..XXX..", "...X..."],
  close: ["X.....X", ".X...X.", "..X.X..", "...X...", "..X.X..", ".X...X.", "X.....X"]
};

const themeData = readThemes();
let lookChange = Promise.resolve();

const weekdays = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"];
const ranks = ["1ST", "2ND", "3RD"];

function el(tag, props, ...children) {
  const node = document.createElement(tag);
  for (const [key, value] of Object.entries(props || {})) {
    if (value === undefined || value === null || value === false) continue;
    if (key === "class") node.className = value;
    else if (key === "text") node.textContent = value;
    else if (key.startsWith("on")) node.addEventListener(key.slice(2), value);
    else node.setAttribute(key, value === true ? "" : value);
  }
  for (const child of children) if (child !== null && child !== undefined) node.append(child);
  return node;
}

function icon(name) {
  const svg = document.createElementNS(svgSpace, "svg");
  svg.setAttribute("viewBox", "0 0 7 7");
  svg.setAttribute("class", "icon");
  svg.setAttribute("aria-hidden", "true");
  let shape = "";
  icons[name].forEach((row, y) => {
    for (let x = 0; x < row.length; ++x) if (row[x] === "X") shape += "M" + x + " " + y + "h1v1h-1z";
  });
  const path = document.createElementNS(svgSpace, "path");
  path.setAttribute("d", shape);
  svg.append(path);
  return svg;
}

function setText(node, text) {
  if (node.textContent !== text) node.textContent = text;
}

function find(rows, key) {
  return (rows.find((row) => row.key === key) || {}).value || "";
}

function duration(seconds) {
  const whole = Math.max(0, Math.floor(Number(seconds) || 0));
  if (whole < 60) return whole + " s";
  if (whole < 3600) return Math.floor(whole / 60) + " min";
  return Math.floor(whole / 3600) + " h " + Math.floor(whole % 3600 / 60) + " min";
}

function shortDuration(seconds) {
  const value = Number(seconds) || 0;
  return value < 100 * 3600 ? duration(value) : Math.floor(value / 3600) + " h";
}

function roughDuration(seconds) {
  const value = Number(seconds) || 0;
  return value < 3600 ? duration(value) : Math.round(value / 3600) + " h";
}

function plural(count, one, many) {
  return count + " " + (Number(count) === 1 ? one : many);
}

function coverUrl(app) {
  return "/cover/" + encodeURIComponent(app) + "?token=" + encodeURIComponent(token);
}

function cover(app, className) {
  const picture = el("img", { src: coverUrl(app), alt: "", loading: "lazy", decoding: "async", width: "460", height: "215" });
  picture.addEventListener("error", () => picture.remove(), { once: true });
  return el("span", { class: "art " + className, "aria-hidden": "true" }, picture);
}

function gameLink(app, name) {
  return el("button", { type: "button", class: "game-link", title: "Details", onclick: () => openGame(app) }, el("span", { class: "name", text: name }));
}

function opensGame(node, app) {
  node.classList.add("opens");
  node.addEventListener("click", (event) => {
    if (!event.target.closest("button, a, input, select, form, label")) openGame(app);
  });
  return node;
}

async function call(words) {
  let response;
  try {
    response = await fetch("/api", {
      method: "POST",
      headers: { "Content-Type": "application/json", "X-Idler-Token": token },
      body: JSON.stringify(words),
      cache: "no-store",
      credentials: "omit",
      redirect: "error"
    });
  } catch (error) {
    throw new Error({{MI-7015}});
  }
  if (response.status === 401) throw new TokenError();
  let data;
  try {
    data = await response.json();
  } catch (error) {
    throw new Error({{MI-7016}}.replace("{}", String(response.status)));
  }
  const columns = Array.isArray(data.columns) ? data.columns : [];
  const rows = Array.isArray(data.rows) ? data.rows.map((row) => Object.fromEntries(columns.map((name, index) => [name, row[index] ?? ""]))) : [];
  return { ok: data.ok === true, text: String(data.text || "").trim(), rows };
}

function showBanner(text) {
  const banner = $("banner");
  banner.textContent = text;
  banner.hidden = false;
}

function hideBanner() {
  $("banner").hidden = true;
}

function toast(text, ok) {
  const box = $("toast");
  box.textContent = text;
  box.classList.toggle("failed", !ok);
  box.classList.add("show");
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => box.classList.remove("show"), ok ? 3500 : 6000);
}

function lockPage() {
  locked = true;
  clearTimeout(timer);
  for (const control of document.querySelectorAll("button:not(#look), input, select")) control.disabled = true;
  setPill("No token", "bad");
  showBanner({{MI-7010}});
}

function trouble(error) {
  if (error instanceof TokenError) {
    lockPage();
    return;
  }
  failing = true;
  setPill("Not answering", "bad");
  $("toggle").disabled = true;
  showBanner(error.message);
}

async function act(words, quiet) {
  if (locked) return { ok: false, text: "", rows: [] };
  try {
    const result = await call(words);
    if (!quiet || !result.ok) toast(result.text || (result.ok ? "Done." : "That did not work."), result.ok);
    return result;
  } catch (error) {
    trouble(error);
    return { ok: false, text: "", rows: [] };
  }
}

function tone(state) {
  const text = state.toLowerCase();
  if (text.startsWith("idling")) return "good";
  if (text.startsWith("failed") || text.startsWith("waiting: steam")) return "bad";
  if (text.startsWith("stopped") || text.startsWith("finished")) return "muted";
  return "warn";
}

function setPill(text, kind) {
  const pill = $("state");
  pill.textContent = text;
  pill.title = text;
  pill.className = "pill " + kind;
}

function fact(label, value) {
  return el("div", { class: "fact" }, el("dt", { text: label }), el("dd", { text: value }));
}

function confirmButton(label, words, done, className) {
  const button = el("button", { type: "button", class: className || "small", text: label });
  let armed = 0;
  const disarm = () => {
    clearTimeout(armed);
    armed = 0;
    button.textContent = label;
    button.classList.remove("armed");
  };
  button.addEventListener("click", async () => {
    if (!armed) {
      button.textContent = "Sure?";
      button.classList.add("armed");
      armed = setTimeout(disarm, 3000);
      return;
    }
    disarm();
    button.disabled = true;
    const result = await act(words, false);
    button.disabled = false;
    if (done) done(result);
  });
  button.addEventListener("blur", () => { if (armed) disarm(); });
  return button;
}

function armStatic(button, words, done) {
  const label = button.textContent;
  let armed = 0;
  const disarm = () => {
    clearTimeout(armed);
    armed = 0;
    button.textContent = label;
    button.classList.remove("armed");
  };
  button.addEventListener("click", async () => {
    if (!armed) {
      button.textContent = "Sure?";
      button.classList.add("armed");
      armed = setTimeout(disarm, 3000);
      return;
    }
    disarm();
    await act(words(), false);
    if (done) done();
  });
  button.addEventListener("blur", () => { if (armed) disarm(); });
}

function ask(options) {
  const dialog = $("ask");
  const field = $("ask-input");
  const note = $("ask-note");
  $("ask-title").textContent = options.title;
  note.textContent = options.note || "";
  note.hidden = !options.note;
  field.hidden = options.input === false;
  field.value = options.value || "";
  $("ask-ok").textContent = options.ok || "OK";
  dialog.showModal();
  if (!field.hidden) {
    field.focus();
    field.select();
  }
  return new Promise((resolve) => {
    const form = $("ask-form");
    const cancel = $("ask-cancel");
    const finish = (answer) => {
      form.removeEventListener("submit", submit);
      cancel.removeEventListener("click", stop);
      dialog.removeEventListener("cancel", stop);
      dialog.close();
      resolve(answer);
    };
    const submit = (event) => {
      event.preventDefault();
      finish(field.hidden ? true : field.value.trim());
    };
    const stop = (event) => {
      if (event) event.preventDefault();
      finish(null);
    };
    form.addEventListener("submit", submit);
    cancel.addEventListener("click", stop);
    dialog.addEventListener("cancel", stop);
  });
}

function goalPicker(kindValue, hoursValue, withUsual) {
  const kind = el("select", { "aria-label": "Goal" },
    withUsual ? el("option", { value: "", text: "The usual goal" }) : null,
    el("option", { value: "none", text: "No limit" }),
    el("option", { value: "for", text: "For hours" }),
    el("option", { value: "until", text: "Until hours played" }));
  kind.value = kindValue;
  const hours = el("input", { type: "number", min: "0.1", step: "any", inputmode: "decimal", value: hoursValue || "3", "aria-label": "Hours" });
  const fit = () => { hours.hidden = kind.value === "" || kind.value === "none"; };
  kind.addEventListener("change", fit);
  fit();
  const words = () => kind.value === "" ? [] : kind.value === "none" ? ["none"] : [kind.value, hours.value];
  return { kind, hours, words };
}

function barChart(values, labels, highlight, fresh) {
  const top = Math.max(0, ...values);
  const chart = el("div", { class: "chart", "data-fresh": fresh });
  values.forEach((value, index) => {
    const fill = el("span");
    fill.style.setProperty("--v", top > 0 ? String(value / top) : "0");
    fill.style.setProperty("--i", String(index));
    chart.append(el("div", { class: "bar-col" + (value > 0 ? "" : " empty") + (index === highlight ? " today" : ""), title: labels[index] + ": " + duration(value) }, fill));
  });
  return chart;
}

function chartBlock(target, values, labels, highlight, first, last, fresh) {
  const top = Math.max(0, ...values);
  if (top <= 0) {
    target.replaceWith(el("div", { id: target.id }, el("p", { class: "quiet", text: "Nothing idled yet." })));
    return;
  }
  target.replaceWith(el("div", { id: target.id },
    el("span", { class: "chart-top", text: top > 0 ? "Top " + shortDuration(top) : "" }),
    barChart(values, labels, highlight, fresh),
    el("div", { class: "axis" }, el("span", { text: first }), el("span", { text: last }))));
}

function drawStatus(rows) {
  statusRows = rows;
  const facts = rows.filter((row) => row.label && row.key !== "slots");
  $("status").replaceChildren(...facts.map((row) => fact(row.label, row.value)));
  const state = find(rows, "state");
  setPill(state || "Unknown", tone(state));
  active = find(rows, "active") === "yes";
  document.documentElement.dataset.active = active ? "yes" : "no";
  const toggle = $("toggle");
  toggle.textContent = active ? "Stop" : "Start";
  toggle.classList.toggle("primary", !active);
  toggle.disabled = false;
  const slots = $("slots");
  if (document.activeElement !== slots) slots.value = find(rows, "slots");
  $("undo").hidden = find(rows, "undo") !== "yes";
  drawAccount(rows);
  if (page === "settings") drawSteamFacts();
}

function drawAccount(rows) {
  const persona = find(rows, "persona");
  const box = $("account");
  box.classList.toggle("signed-in", Boolean(persona));
  box.replaceChildren(el("small", { text: "Steam" }),
    persona ? el("strong", { class: "account-name", text: persona, title: persona }) : null,
    persona ? el("span", { class: "account-state good", text: "Logged on" }) : el("span", { class: "account-note", text: find(rows, "steam") || "unknown" }));
}

function goalCell(item) {
  const entry = item.entry;
  if (editing !== entry.app) {
    return el("button", { type: "button", class: "link", text: entry.goal, title: "Change the goal", "aria-label": "Goal of " + entry.name + ": " + entry.goal + ". Change it", onclick: () => { editing = entry.app; drawQueue(); } });
  }
  const picker = goalPicker(entry.kind === "for" || entry.kind === "until" ? entry.kind : "none", entry.hours, false);
  const close = () => { editing = null; drawQueue(); };
  const form = el("form", { class: "goal-edit" }, picker.kind, picker.hours,
    el("button", { type: "submit", class: "small primary", text: "Save" }),
    el("button", { type: "button", class: "small", text: "Cancel", onclick: close }));
  form.addEventListener("submit", async (event) => {
    event.preventDefault();
    const result = await act(["goal", entry.app, ...picker.words()], true);
    if (result.ok) {
      close();
      refresh();
    }
  });
  form.addEventListener("keydown", (event) => { if (event.key === "Escape") close(); });
  setTimeout(() => picker.kind.focus(), 0);
  return form;
}

function iconButton(name, label, onclick) {
  return el("button", { type: "button", class: "small icon-button", title: label, "aria-label": label, onclick }, icon(name));
}

function entryActions(item, index, count, flags) {
  const app = item.entry.app;
  const after = () => refresh();
  const up = iconButton("up", "Move up", async () => { await act(["move", app, String(index)], true); after(); });
  const down = iconButton("down", "Move down", async () => { await act(["move", app, String(index + 2)], true); after(); });
  up.disabled = index === 0;
  down.disabled = index === count - 1;
  const buttons = [up, down];
  if (flags.includes("failed")) buttons.push(el("button", { type: "button", class: "small", text: "Try again", onclick: async () => { await act(["retry", app], false); after(); } }));
  if (flags.includes("held")) buttons.push(confirmButton("Past refund", ["refund", app], after));
  if (!flags.includes("running") && !flags.includes("failed") && !flags.includes("held")) buttons.push(el("button", { type: "button", class: "small", text: "Idle now", onclick: async () => { await act(["now", app], false); after(); } }));
  buttons.push(confirmButton("Remove", ["remove", app], () => { markQueued(app, false); after(); }));
  return buttons;
}

function makeEntry(entry) {
  const item = { entry, editing: false, actionSignature: "" };
  item.name = el("span", { class: "name" });
  item.app = el("span", { class: "app" });
  item.state = el("span", { class: "state" });
  item.goal = el("span", { class: "goal" });
  item.idled = el("span", { class: "meta" });
  item.left = el("span", { class: "meta" });
  item.fill = el("span");
  item.meter = el("div", { class: "meter", role: "progressbar", "aria-valuemin": "0", "aria-valuemax": "100" }, item.fill);
  item.actions = el("div", { class: "entry-actions" });
  item.open = el("button", { type: "button", class: "entry-open", onclick: () => openGame(item.entry.app) }, item.name);
  item.li = el("li", { class: "entry" }, cover(entry.app, "thumb"),
    el("div", { class: "entry-main" },
      el("div", { class: "entry-title" }, item.open, item.app),
      el("div", { class: "entry-line" }, item.state, item.goal, item.idled, item.left),
      item.meter),
    item.actions);
  opensGame(item.li, entry.app);
  return item;
}

function fillEntry(item, entry, index, count) {
  const goalChanged = item.entry.goal !== entry.goal || item.entry.kind !== entry.kind || item.entry.hours !== entry.hours;
  const editingNow = editing === entry.app;
  const flags = entry.flags.split(" ").filter(Boolean);
  item.entry = entry;
  setText(item.name, entry.name);
  item.open.setAttribute("aria-label", entry.name + ": details");
  setText(item.app, entry.app);
  setText(item.state, entry.state);
  item.state.className = "state " + tone(entry.state);
  setText(item.idled, "Idled " + entry.idled);
  setText(item.left, entry.left ? entry.left + " left" : "");
  item.left.hidden = !entry.left;
  item.meter.hidden = entry.progress === "";
  if (entry.progress !== "") {
    const share = Math.max(0, Math.min(1, Number(entry.progress) || 0));
    item.fill.style.setProperty("--p", String(share));
    item.meter.setAttribute("aria-valuenow", String(Math.round(share * 100)));
    item.meter.setAttribute("aria-label", "Goal of " + entry.name + ": " + Math.round(share * 100) + " %");
  }
  item.li.className = "entry" + flags.map((flag) => " " + flag).join("");
  if (!item.goal.firstChild || item.editing !== editingNow || (!editingNow && goalChanged)) {
    item.goal.replaceChildren(goalCell(item));
    item.editing = editingNow;
  }
  const signature = index + "|" + count + "|" + entry.flags;
  if (item.actionSignature !== signature) {
    item.actionSignature = signature;
    item.actions.replaceChildren(...entryActions(item, index, count, flags));
  }
}

function drawQueue() {
  const list = $("queue");
  const wanted = new Set(queueEntries.map((entry) => entry.app));
  for (const [app, item] of queueItems) {
    if (wanted.has(app)) continue;
    item.li.remove();
    queueItems.delete(app);
    if (editing === app) editing = null;
  }
  queueEntries.forEach((entry, index) => {
    let item = queueItems.get(entry.app);
    if (!item) {
      item = makeEntry(entry);
      item.li.style.setProperty("--i", String(index));
      queueItems.set(entry.app, item);
    }
    fillEntry(item, entry, index, queueEntries.length);
    if (list.children[index] !== item.li) list.insertBefore(item.li, list.children[index] || null);
  });
  list.hidden = queueEntries.length === 0;
  $("queue-empty").hidden = queueEntries.length !== 0;
}

function drawLists(rows) {
  listRows = rows;
  const select = $("lists");
  const signature = JSON.stringify(rows);
  if (signature === listSignature || document.activeElement === select) return;
  listSignature = signature;
  select.replaceChildren(...rows.map((row) => {
    const option = el("option", { value: row.name, text: row.name + " (" + row.games + ")" });
    option.selected = row.active === "yes";
    return option;
  }));
  select.disabled = rows.length < 2;
  $("list-delete").disabled = rows.length < 2;
}

function currentList() {
  return (listRows.find((row) => row.active === "yes") || {}).name || "";
}

async function loadQueue() {
  const [queue, lists] = await Promise.all([call(["queue"]), call(["lists"])]);
  queueEntries = queue.rows;
  drawQueue();
  drawLists(lists.rows);
}

function libraryWords() {
  return ["library", $("lib-filter").value, $("lib-sort").value, $("lib-rating").value, $("lib-search").value.trim()];
}

function tile(game) {
  const button = el("button", { type: "button", class: "tile", "aria-pressed": String(game.app === selected), "data-app": game.app },
    cover(game.app, "tile-art"),
    el("span", { class: "tile-name", text: game.name }),
    el("span", { class: "tile-meta" }, el("span", { text: game.playtime }), game.queued === "yes" ? el("span", { class: "tag", text: "In queue" }) : null));
  button.addEventListener("click", () => selectGame(game.app));
  return button;
}

function watchMore() {
  moreWatcher.unobserve($("lib-more"));
  moreWatcher.observe($("lib-more"));
}

function showMoreGames() {
  const next = library.slice(libraryShown, libraryShown + libraryStep);
  $("lib-grid").append(...next.map((game, index) => {
    const node = tile(game);
    node.style.setProperty("--i", String(index));
    return node;
  }));
  libraryShown += next.length;
  if (libraryShown < library.length) watchMore();
}

const moreWatcher = new IntersectionObserver((entries) => {
  if (entries.some((entry) => entry.isIntersecting) && page === "library" && libraryShown < library.length) showMoreGames();
}, { rootMargin: "800px" });

async function loadLibrary(force) {
  clearTimeout(librarySearchTimer);
  const words = libraryWords();
  const key = JSON.stringify(words);
  if (!force && key === libraryLoaded) return;
  const number = ++libraryNumber;
  const result = await call(words);
  if (number !== libraryNumber) return;
  const grid = $("lib-grid");
  if (!result.ok) {
    library = [];
    libraryLoaded = "";
    grid.replaceChildren();
    $("lib-count").textContent = result.text;
    librarySearchTimer = setTimeout(() => { if (page === "library") loadLibrary(true).catch(trouble); }, retryMilliseconds);
    return;
  }
  libraryLoaded = key;
  library = result.rows;
  libraryShown = 0;
  grid.replaceChildren();
  $("lib-count").textContent = plural(library.length, "game", "games");
  showMoreGames();
}

function markQueued(app, queued) {
  const game = library.find((row) => row.app === app);
  if (!game) return;
  game.queued = queued ? "yes" : "no";
  const old = $("lib-grid").querySelector('.tile[data-app="' + CSS.escape(app) + '"]');
  if (old) old.replaceWith(tile(game));
}

async function gameData(app) {
  const [game, queue] = await Promise.all([call(["game", app]), call(["queue"])]);
  return { game, queue: queue.rows };
}

async function selectGame(app) {
  selected = app;
  for (const node of $("lib-grid").querySelectorAll(".tile")) node.setAttribute("aria-pressed", String(node.dataset.app === app));
  const panel = $("detail");
  panel.classList.remove("closed");
  const number = ++detailNumber;
  let data;
  try {
    data = await gameData(app);
  } catch (error) {
    trouble(error);
    return;
  }
  if (number !== detailNumber) return;
  if (!data.game.ok) {
    panel.replaceChildren(el("p", { class: "quiet", text: data.game.text }));
    return;
  }
  panel.replaceChildren(...gameView(app, data, () => panel.classList.add("closed"), () => selectGame(app)));
  panel.firstChild.classList.add("detail-close");
}

async function openGame(app) {
  const panel = $("game-panel");
  const first = panel.hidden;
  const number = ++panelNumber;
  if (first) {
    panel.replaceChildren(closeButton(closeGame), el("p", { class: "quiet", text: "Loading." }));
    panel.hidden = false;
    $("scrim").hidden = false;
  }
  let data;
  try {
    data = await gameData(app);
  } catch (error) {
    trouble(error);
    return;
  }
  if (number !== panelNumber || panel.hidden) return;
  if (!data.game.ok) {
    panel.replaceChildren(closeButton(closeGame), el("p", { class: "quiet", text: data.game.text }));
    return;
  }
  panel.replaceChildren(...gameView(app, data, closeGame, () => openGame(app)));
  if (first) panel.firstChild.focus({ preventScroll: true });
}

function closeGame() {
  ++panelNumber;
  $("game-panel").hidden = true;
  $("scrim").hidden = true;
}

function closeButton(close) {
  const button = iconButton("close", "Close", close);
  button.classList.add("panel-close");
  return button;
}

function addControls(app, again) {
  const picker = goalPicker("", "3", true);
  const add = el("button", { type: "button", class: "primary", text: "Add to the queue" });
  add.addEventListener("click", async () => {
    add.disabled = true;
    const result = await act(["add", ...picker.words(), "--", app], false);
    if (result.ok) markQueued(app, true);
    refresh();
    again();
  });
  const now = el("button", { type: "button", text: "Idle now", onclick: async () => {
    const result = await act(["now", app], false);
    if (result.ok) markQueued(app, true);
    refresh();
    again();
  } });
  return el("div", { class: "detail-actions" }, el("div", { class: "goal-edit" }, picker.kind, picker.hours), add, now);
}

function queueControls(app, queue, again) {
  const index = queue.findIndex((row) => row.app === app);
  if (index < 0) return null;
  const entry = queue[index];
  const flags = entry.flags.split(" ").filter(Boolean);
  const after = () => {
    refresh();
    again();
  };
  const run = async (words, quiet) => {
    await act(words, quiet);
    after();
  };
  const up = el("button", { type: "button", class: "small", onclick: () => run(["move", app, String(index)], true) }, icon("up"), "Up");
  const down = el("button", { type: "button", class: "small", onclick: () => run(["move", app, String(index + 2)], true) }, icon("down"), "Down");
  const top = el("button", { type: "button", class: "small", text: "To the top", onclick: () => run(["move", app, "1"], true) });
  up.disabled = index === 0;
  top.disabled = index === 0;
  down.disabled = index === queue.length - 1;
  const picker = goalPicker(entry.kind === "for" || entry.kind === "until" ? entry.kind : "none", entry.hours, false);
  const goal = el("form", { class: "goal-edit" }, picker.kind, picker.hours, el("button", { type: "submit", class: "small", text: "Set goal" }));
  goal.addEventListener("submit", (event) => {
    event.preventDefault();
    run(["goal", app, ...picker.words()], false);
  });
  const actions = [];
  if (flags.includes("failed")) actions.push(el("button", { type: "button", text: "Try again", onclick: () => run(["retry", app], false) }));
  if (flags.includes("held")) actions.push(confirmButton("Past refund", ["refund", app], after, "wide"));
  if (!flags.includes("running") && !flags.includes("failed") && !flags.includes("held")) actions.push(el("button", { type: "button", class: "primary", text: "Idle now", onclick: () => run(["now", app], false) }));
  actions.push(confirmButton("Remove from the queue", ["remove", app], () => { markQueued(app, false); after(); }, "wide"));
  const where = "Place " + (index + 1) + " of " + queue.length + ", " + entry.state + (entry.left ? ", " + entry.left + " left" : "");
  return el("section", { class: "detail-queue" + (flags.includes("running") ? " running" : "") },
    el("h2", { text: "In the queue" }),
    el("p", { class: "quiet", text: where }),
    el("div", { class: "detail-buttons" }, up, down, top),
    goal,
    el("div", { class: "detail-actions" }, ...actions));
}

function gameView(app, data, close, again) {
  const rows = data.game.rows;
  const queued = find(rows, "queued") === "yes";
  const facts = rows.filter((row) => row.label && row.key !== "name" && row.key !== "day");
  const days = rows.filter((row) => row.key === "day");
  const values = days.map((row) => Number(row.value) || 0);
  const chart = el("div");
  const holder = el("div", { class: "detail-chart" }, chart);
  chartBlock(chart, values, days.map((row) => row.label), values.length - 1, days.length ? days[0].label.slice(5) : "", "Today", true);
  return [
    closeButton(close),
    cover(app, "detail-art"),
    el("h3", { text: find(rows, "name") }),
    (queued ? queueControls(app, data.queue, again) : null) || addControls(app, again),
    el("dl", { class: "facts" }, ...facts.map((row) => fact(row.label, row.value))),
    el("h2", { text: "Idled, last 30 days" }),
    holder
  ];
}

function series(rows, name) {
  return rows.filter((row) => row.series === name);
}

function statTile(label, value, note) {
  return el("div", { class: "tile-stat" }, el("small", { text: label }), el("strong", { text: value, title: value }), el("span", { text: note }));
}

function drawOverview(rows, fresh) {
  $("stats-overview").toggleAttribute("data-fresh", Boolean(fresh));
  const tiles = series(rows, "tile");
  const number = (key) => Number(find(tiles, key)) || 0;
  const extra = (list, key) => (list.find((row) => row.key === key) || {}).extra || "";
  const records = series(rows, "record");
  const record = (key) => Number(find(records, key)) || 0;
  const week = number("week");
  const before = number("weekbefore");
  const trend = before > 0 ? (week >= before ? "+" : "") + Math.round((week / before - 1) * 100) + " %" : week > 0 ? "Nothing before" : "";
  const first = extra(tiles, "total");
  $("tiles").replaceChildren(
    statTile("Today", shortDuration(number("today")), "Yesterday " + roughDuration(number("yesterday"))),
    statTile("Last 7 days", shortDuration(week), trend),
    statTile("All time", shortDuration(number("total")), first ? "Since " + first : "Nothing yet"),
    statTile("Games", String(number("games")), "On " + plural(number("days"), "day", "days")),
    statTile("Sessions", String(number("sessions")), record("longest") > 0 ? "Longest " + roughDuration(record("longest")) : "None yet"),
    statTile("Days in a row", String(number("streak")), "Best " + number("beststreak")));

  const days = series(rows, "day");
  const month = days.slice(-30);
  chartBlock($("chart-days"), month.map((row) => Number(row.value) || 0), month.map((row) => row.key), month.length - 1, month.length ? month[0].key.slice(5) : "", "Today", fresh);

  const podium = series(rows, "top");
  $("podium").replaceChildren(...podium.map((row, index) => el("li", {},
    el("button", { type: "button", class: "podium-item", onclick: () => openGame(row.key) },
      el("span", { class: "rank", text: ranks[index] }),
      el("span", { class: "podium-text" }, el("span", { class: "name", text: row.extra }), el("span", { class: "meta", text: shortDuration(row.value) }))))));
  if (!podium.length) $("podium").append(el("li", {}, el("span", { class: "quiet", text: "Nothing idled yet." })));

  drawCalendar(days);

  const recordList = $("records");
  if (!records.length) {
    recordList.replaceChildren(el("p", { class: "quiet", text: "Nothing idled yet." }));
  } else {
    recordList.replaceChildren(
      fact("Best day", shortDuration(record("bestday")) + ", " + extra(records, "bestday")),
      fact("Most games", record("mostgames") + " on " + extra(records, "mostgames")),
      fact("Most in a row", plural(number("beststreak"), "day", "days")),
      fact("Average day", shortDuration(record("average"))),
      record("longest") > 0 ? fact("Longest run", shortDuration(record("longest")) + ", " + extra(records, "longest")) : "",
      record("mostsessions") > 0 ? fact("Most sessions", record("mostsessions") + ", " + extra(records, "mostsessions")) : "");
  }

  const weekdayRows = series(rows, "weekday");
  chartBlock($("chart-weekdays"), weekdayRows.map((row) => Number(row.value) || 0), weekdays, -1, "Monday", "Sunday", fresh);
  const hourRows = series(rows, "hour");
  chartBlock($("chart-hours"), hourRows.map((row) => Number(row.value) || 0), hourRows.map((row) => row.key + ":00"), new Date().getHours(), "0:00", "23:00", fresh);
}

function drawCalendar(days) {
  const calendar = $("calendar");
  const values = days.map((row) => Number(row.value) || 0);
  const top = Math.max(0, ...values);
  const cells = [];
  if (days.length) {
    const [year, month, day] = days[0].key.split("-").map(Number);
    const offset = (new Date(year, month - 1, day).getDay() + 6) % 7;
    for (let index = 0; index < offset; ++index) cells.push(el("span", { class: "blank", "aria-hidden": "true" }));
  }
  days.forEach((row, index) => {
    const value = values[index];
    const level = value <= 0 || top <= 0 ? 0 : value >= top * 0.75 ? 4 : value >= top * 0.5 ? 3 : value >= top * 0.25 ? 2 : 1;
    cells.push(el("span", { class: level ? "l" + level : "", title: row.key + ": " + duration(value) }));
  });
  calendar.replaceChildren(...cells);
  const wrap = calendar.parentElement;
  wrap.scrollLeft = wrap.scrollWidth;
}

function drawGameFigures(rows) {
  $("stats-table").replaceChildren(...rows.map((row) => opensGame(el("tr", {},
    el("td", {}, gameLink(row.app, row.name), el("br"), el("span", { class: "app", text: row.app })),
    el("td", { class: "num", "data-label": "Idled", text: shortDuration(row.seconds) }),
    el("td", { class: "num", "data-label": "Sessions", text: row.sessions }),
    el("td", { class: "num", "data-label": "Longest", text: shortDuration(row.longest) }),
    el("td", { class: "num", "data-label": "Days", text: row.days }),
    el("td", { "data-label": "Last", text: row.last })), row.app)));
  $("stats-empty").hidden = rows.length !== 0;
}

async function loadDay(day) {
  historyDay = day;
  for (const button of $("history-days").querySelectorAll("button")) button.setAttribute("aria-pressed", String(button.dataset.day === day));
  const result = await call(["history", day]);
  $("history-title").textContent = day;
  $("history-games").replaceChildren(...result.rows.map((row) => opensGame(el("li", {}, gameLink(row.app, row.name), el("span", { class: "meta", text: shortDuration(row.seconds) })), row.app)));
  if (!result.rows.length) $("history-games").append(el("li", {}, el("span", { class: "quiet", text: result.text })));
}

async function loadStats(fresh) {
  statsAt = Date.now();
  for (const tab of document.querySelectorAll(".tabs [data-tab]")) tab.setAttribute("aria-selected", String(tab.dataset.tab === statsTab));
  $("stats-overview").hidden = statsTab !== "overview";
  $("stats-games").hidden = statsTab !== "games";
  $("stats-history").hidden = statsTab !== "history";
  if (statsTab === "overview") {
    drawOverview((await call(["stats"])).rows, fresh);
  } else if (statsTab === "games") {
    drawGameFigures((await call(["stats", "games", $("stats-order").value])).rows);
  } else {
    const result = await call(["history"]);
    $("history-days").replaceChildren(...result.rows.map((row) => el("li", {},
      el("button", { type: "button", "data-day": row.day, "aria-pressed": "false", onclick: () => loadDay(row.day).catch(trouble) },
        el("span", { text: row.day }), el("span", { text: shortDuration(row.seconds) }), el("span", { class: "meta", text: plural(row.games, "game", "games") })))));
    if (!result.rows.length) {
      $("history-days").append(el("li", {}, el("span", { class: "quiet", text: "Nothing idled yet." })));
      $("history-games").replaceChildren();
      return;
    }
    await loadDay(result.rows.some((row) => row.day === historyDay) ? historyDay : result.rows[0].day);
  }
}

function drawLog(stick) {
  const list = $("log");
  const level = $("log-level").value;
  const search = $("log-search").value.trim().toLowerCase();
  const shown = logRows.filter((row) => (level === "all" || row.level === "ERROR" || (level === "warn" && row.level === "WARN")) &&
    (!search || row.message.toLowerCase().includes(search)));
  const tones = { ERROR: "bad", WARN: "warn", SUCCESS: "good" };
  list.replaceChildren(...shown.map((row) => {
    const code = row.message.match(/\((MI-\d{4})\)\s*$/);
    const text = code ? row.message.slice(0, code.index) : row.message;
    return el("li", {},
      el("span", { class: "when", text: row.time }),
      el("span", { class: "level " + (tones[row.level] || "muted"), text: row.level }),
      el("span", { class: "text" }, text, code ? el("span", { class: "code", text: "(" + code[1] + ")" }) : null));
  }));
  if (!shown.length) list.append(el("li", {}, el("span", { class: "text muted", text: logRows.length ? "No entry fits." : "Nothing yet." })));
  if (stick) list.scrollTop = list.scrollHeight;
}

async function loadLog() {
  const list = $("log");
  const atEnd = list.scrollTop + list.clientHeight >= list.scrollHeight - 8;
  logRows = (await call(["log", $("log-lines").value])).rows;
  drawLog(atEnd);
}

const settingGroups = {
  idling: ["pause", "resume", "awake", "refund", "invisible", "vary", "spread"],
  when: ["away", "awayminutes", "hours", "from", "until"],
  goal: ["goal"]
};
const settingParents = { spread: "vary", awayminutes: "away", from: "hours", until: "hours" };
const settingUnits = { spread: "%", awayminutes: "min", from: ":00", until: ":00" };

async function saveSetting(key, values) {
  await act(["set", key, ...values], false);
  await loadSettings();
}

function settingControl(row) {
  if (row.kind === "bool") {
    const on = row.value === "yes";
    return [el("button", { type: "button", class: "switch", role: "switch", "aria-checked": String(on), "aria-label": row.label, onclick: () => saveSetting(row.key, [on ? "off" : "on"]) })];
  }
  if (row.kind === "goal") {
    const [kind, hours] = row.value.split(" ");
    const picker = goalPicker(kind || "none", hours, false);
    const save = () => saveSetting("goal", picker.words());
    picker.kind.addEventListener("change", save);
    picker.hours.addEventListener("change", save);
    return [picker.kind, picker.hours];
  }
  const input = el("input", { type: "number", min: row.min, max: row.max, step: row.step, value: row.value, inputmode: "decimal", "aria-label": row.label });
  input.addEventListener("change", () => saveSetting(row.key, [input.value]));
  return settingUnits[row.key] ? [input, el("span", { class: "muted", text: settingUnits[row.key] })] : [input];
}

function drawSettings() {
  const value = (key) => find(settingsRows, key);
  for (const [group, keys] of Object.entries(settingGroups)) {
    const box = $("set-" + group);
    const rows = keys.map((key) => settingsRows.find((row) => row.key === key)).filter(Boolean);
    box.closest(".card").hidden = rows.length === 0;
    box.replaceChildren(...rows.filter((row) => !settingParents[row.key] || value(settingParents[row.key]) === "yes").map((row) =>
      el("div", { class: "setting" + (settingParents[row.key] ? " sub" : "") },
        el("strong", { text: row.label }),
        el("p", { text: row.note }),
        el("div", { class: "control" }, ...settingControl(row)))));
  }
}

function drawSteamFacts() {
  $("steam-facts").replaceChildren(...statusRows.filter((row) => row.key === "steam" || row.key === "library").map((row) => fact(row.label, row.value)));
}

function readThemes() {
  let data = {};
  try { data = JSON.parse($("themes").textContent); } catch (error) {}
  return {
    fallback: typeof data.default === "string" ? data.default : "",
    folder: typeof data.folder === "string" ? data.folder : "",
    docs: typeof data.docs === "string" && data.docs.startsWith("https://") ? data.docs : "",
    themes: Array.isArray(data.themes) ? data.themes : [],
    problems: Array.isArray(data.problems) ? data.problems : []
  };
}

function stored(key) {
  try { return localStorage.getItem(key) || ""; } catch (error) { return ""; }
}

function store(key, value) {
  try { localStorage.setItem(key, value); } catch (error) {}
}

function themeOf(id) {
  return themeData.themes.find((theme) => theme.id === id);
}

function currentLook() {
  const root = document.documentElement;
  return { theme: root.dataset.theme || "", variant: root.dataset.variant || "", options: (root.dataset.options || "").split(" ").filter(Boolean) };
}

function optionsOf(theme) {
  const saved = stored("manifold-idler.options." + theme.id);
  const on = saved.startsWith("on:") ? saved.slice(3).split(" ") : null;
  return theme.options.filter((option) => on ? on.includes(option.id) : option.on).map((option) => option.id);
}

function lookName(theme, variant) {
  return variant && theme.variants.length > 1 ? theme.name + " · " + variant.name : theme.name;
}

function lookStops() {
  return themeData.themes.flatMap((theme) => theme.variants.length ? theme.variants.map((variant) => ({ theme, variant })) : [{ theme, variant: null }]);
}

function themeSheet(id) {
  return new Promise((resolve) => {
    const link = el("link", { rel: "stylesheet", href: "/themes/" + encodeURIComponent(token) + "/" + encodeURIComponent(id) + "/theme.css" });
    link.addEventListener("load", () => resolve(link), { once: true });
    link.addEventListener("error", () => resolve(link), { once: true });
    document.head.append(link);
  });
}

function withTransition(change) {
  if (!document.startViewTransition || window.matchMedia("(prefers-reduced-motion: reduce)").matches) return change();
  return document.startViewTransition(change).updateCallbackDone.catch(() => {});
}

function setLook(themeId, variantId) {
  const theme = themeOf(themeId);
  if (!theme) return;
  const variant = theme.variants.find((one) => one.id === variantId) || theme.variants.find((one) => one.id === stored("manifold-idler.variant." + theme.id)) || theme.variants[0];
  store("manifold-idler.theme", theme.id);
  if (variant) store("manifold-idler.variant." + theme.id, variant.id);
  const apply = async () => {
    const root = document.documentElement;
    if (root.dataset.theme !== theme.id || !$("theme-sheet")) {
      const link = await themeSheet(theme.id);
      const old = $("theme-sheet");
      if (old) old.remove();
      link.id = "theme-sheet";
    }
    root.dataset.theme = theme.id;
    if (variant) root.dataset.variant = variant.id;
    else delete root.dataset.variant;
    root.dataset.options = optionsOf(theme).join(" ");
    nameLook();
    drawLooks();
  };
  lookChange = lookChange.then(() => withTransition(apply)).catch(() => {});
}

function nextLook() {
  const stops = lookStops();
  if (!stops.length) return;
  const now = currentLook();
  const at = stops.findIndex((stop) => stop.theme.id === now.theme && (!stop.variant || stop.variant.id === now.variant));
  const next = stops[(at + 1) % stops.length];
  setLook(next.theme.id, next.variant ? next.variant.id : "");
}

function setOption(theme, id, on) {
  const chosen = new Set(optionsOf(theme));
  if (on) chosen.add(id);
  else chosen.delete(id);
  const list = theme.options.map((option) => option.id).filter((option) => chosen.has(option));
  store("manifold-idler.options." + theme.id, "on:" + list.join(" "));
  if (document.documentElement.dataset.theme === theme.id) document.documentElement.dataset.options = list.join(" ");
  drawLooks();
}

function nameLook() {
  const now = currentLook();
  const theme = themeOf(now.theme);
  const variant = theme ? theme.variants.find((one) => one.id === now.variant) : null;
  const name = theme ? lookName(theme, variant) : "Plain";
  const button = $("look");
  button.title = "Look: " + name;
  button.setAttribute("aria-label", "Look: " + name + ". Switch to the next one");
  const colors = variant && variant.swatch.length ? variant.swatch : theme ? theme.swatch : [];
  [...button.children].forEach((square, index) => {
    if (colors.length) square.style.setProperty("background", colors[index % colors.length]);
    else square.style.removeProperty("background");
  });
}

function swatchOf(colors) {
  const swatch = el("span", { class: "swatch", "aria-hidden": "true" });
  for (const color of colors || []) {
    const square = el("i");
    square.style.setProperty("background", color);
    swatch.append(square);
  }
  return swatch;
}

function optionRow(theme, option, on) {
  return el("div", { class: "setting" },
    el("strong", { text: option.name }),
    option.note ? el("p", { text: option.note }) : null,
    el("div", { class: "control" }, el("button", { type: "button", class: "switch", role: "switch", "aria-checked": String(on), "aria-label": option.name, onclick: () => setOption(theme, option.id, !on) })));
}

function drawLooks() {
  const now = currentLook();
  const groups = themeData.themes.map((theme) => {
    const current = theme.id === now.theme;
    const stops = theme.variants.length ? theme.variants : [null];
    return el("div", { class: "look-group", "data-theme": theme.id },
      el("div", { class: "look-head" }, el("strong", { text: theme.name }), theme.from !== "built-in" ? el("span", { class: "tag", text: "Own" }) : null),
      theme.note ? el("p", { class: "look-note", text: theme.note }) : null,
      el("div", { class: "look-grid" }, ...stops.map((variant) => el("button", {
        type: "button",
        "aria-pressed": String(current && (!variant || variant.id === now.variant)),
        onclick: () => setLook(theme.id, variant ? variant.id : "")
      }, swatchOf(variant ? variant.swatch : theme.swatch), el("span", { text: variant ? variant.name : theme.name })))),
      current && theme.options.length ? el("div", { class: "settings" }, ...theme.options.map((option) => optionRow(theme, option, now.options.includes(option.id)))) : null);
  });
  const folder = themeData.folder ? el("p", { class: "quiet look-folder" }, "Themes of your own go into a folder each in ", el("code", { text: themeData.folder }), ".",
    themeData.docs ? " " : null, themeData.docs ? el("a", { href: themeData.docs, target: "_blank", rel: "noopener noreferrer", text: "How to make one" }) : null) : null;
  $("looks").replaceChildren(...groups, folder, ...themeData.problems.map((text) => el("p", { class: "warn look-problem", text: String(text) })));
}

async function loadSettings() {
  settingsRows = (await call(["settings"])).rows;
  drawSettings();
  drawSteamFacts();
  drawLooks();
}

const pages = {
  idling: { title: "Idling", load: loadQueue },
  library: { title: "Library", load: () => loadLibrary(false) },
  statistics: { title: "Statistics", load: () => loadStats(true) },
  log: { title: "Log", load: loadLog },
  settings: { title: "Settings", load: loadSettings }
};

function route() {
  closeGame();
  const wanted = location.hash.replace(/^#/, "");
  page = Object.prototype.hasOwnProperty.call(pages, wanted) ? wanted : "idling";
  for (const section of document.querySelectorAll(".page")) section.hidden = section.dataset.page !== page;
  for (const link of document.querySelectorAll(".nav a")) {
    if (link.dataset.page === page) link.setAttribute("aria-current", "page");
    else link.removeAttribute("aria-current");
  }
  $("page-title").textContent = pages[page].title;
  document.title = pages[page].title + " - Manifold Idler";
  if (!locked && token) pages[page].load().catch(trouble);
}

function schedule() {
  clearTimeout(timer);
  if (!locked && !document.hidden) timer = setTimeout(refresh, failing ? retryMilliseconds : pollMilliseconds);
}

async function refresh() {
  if (locked) return;
  if (refreshing) {
    again = true;
    return;
  }
  refreshing = true;
  try {
    const status = await call(["status"]);
    const wasFailing = failing;
    failing = false;
    hideBanner();
    drawStatus(status.rows);
    if (page === "idling") await loadQueue();
    else if (page === "log") await loadLog();
    else if (page === "statistics" && statsTab === "overview" && Date.now() - statsAt > statsMilliseconds) await loadStats(false);
    if (wasFailing && page !== "idling" && page !== "log") pages[page].load().catch(trouble);
  } catch (error) {
    trouble(error);
  } finally {
    refreshing = false;
    if (again) {
      again = false;
      refresh();
    } else {
      schedule();
    }
  }
}

for (const link of document.querySelectorAll(".nav a[data-icon]")) link.prepend(icon(link.dataset.icon));
if (window.matchMedia("(max-width: 1100px)").matches) $("detail").classList.add("closed");
try { if (localStorage.getItem("manifold-idler.library-view") === "list") $("lib-grid").classList.add("list"); } catch (error) {}
$("lib-view").textContent = $("lib-grid").classList.contains("list") ? "Covers" : "List";

$("toggle").addEventListener("click", async () => {
  $("toggle").disabled = true;
  await act([active ? "stop" : "start"]);
  refresh();
});

$("look").addEventListener("click", nextLook);

$("slots-form").addEventListener("submit", async (event) => {
  event.preventDefault();
  const slots = $("slots");
  await act(["slots", slots.value]);
  slots.blur();
  refresh();
});

$("lists").addEventListener("change", async () => {
  const select = $("lists");
  await act(["use", select.value]);
  select.blur();
  listSignature = "";
  libraryLoaded = "";
  refresh();
});

$("list-new").addEventListener("click", async () => {
  const name = await ask({ title: "New list", note: "It becomes the queue.", ok: "Create" });
  if (!name) return;
  await act(["list", "new", name]);
  listSignature = "";
  refresh();
});

$("list-rename").addEventListener("click", async () => {
  const old = currentList();
  const name = await ask({ title: "Rename the list", value: old, ok: "Rename" });
  if (!name || name === old) return;
  await act(["list", "rename", old, name]);
  listSignature = "";
  refresh();
});

$("list-delete").addEventListener("click", async () => {
  const name = currentList();
  const sure = await ask({ title: "Delete " + name + "?", note: "Its games go with it. Undo brings them back.", input: false, ok: "Delete" });
  if (!sure) return;
  await act(["list", "delete", name]);
  listSignature = "";
  refresh();
});

$("sort").addEventListener("change", async () => {
  const select = $("sort");
  if (!select.value) return;
  await act(["sort", select.value]);
  select.value = "";
  refresh();
});

armStatic($("clear-finished"), () => ["clear", "finished"], () => { libraryLoaded = ""; refresh(); });
armStatic($("clear-all"), () => ["clear", "all"], () => { libraryLoaded = ""; refresh(); });

$("undo").addEventListener("click", async () => {
  await act(["undo"]);
  libraryLoaded = "";
  refresh();
});

$("lib-search").addEventListener("input", () => {
  clearTimeout(librarySearchTimer);
  librarySearchTimer = setTimeout(() => loadLibrary(false).catch(trouble), 250);
});
for (const id of ["lib-filter", "lib-rating", "lib-sort"]) $(id).addEventListener("change", () => loadLibrary(false).catch(trouble));
$("lib-view").addEventListener("click", () => {
  const list = $("lib-grid").classList.toggle("list");
  $("lib-view").textContent = list ? "Covers" : "List";
  try { localStorage.setItem("manifold-idler.library-view", list ? "list" : "covers"); } catch (error) {}
});
moreWatcher.observe($("lib-more"));

for (const tab of document.querySelectorAll(".tabs [data-tab]")) {
  tab.addEventListener("click", () => {
    statsTab = tab.dataset.tab;
    loadStats(true).catch(trouble);
  });
}
$("stats-order").addEventListener("change", () => loadStats(false).catch(trouble));

$("log-level").addEventListener("change", () => drawLog(true));
$("log-search").addEventListener("input", () => drawLog(true));
$("log-lines").addEventListener("change", () => loadLog().catch(trouble));

$("rescan").addEventListener("click", async () => {
  await act(["rescan"]);
  libraryLoaded = "";
  refresh();
});

document.addEventListener("keydown", (event) => {
  if (event.key !== "Escape" || $("ask").open) return;
  $("detail").classList.add("closed");
  closeGame();
});
window.addEventListener("hashchange", route);
$("scrim").addEventListener("click", closeGame);
document.addEventListener("visibilitychange", () => {
  if (document.hidden) clearTimeout(timer);
  else refresh();
});

nameLook();
drawLooks();
if (!token) {
  lockPage();
  route();
} else {
  route();
  refresh();
}
})();

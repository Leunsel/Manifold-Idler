#include "Web.h"

#include <array>
#include <string_view>

namespace Idler::Cli
{
    namespace
    {
        constexpr std::string_view kHead = R"PAGE(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="color-scheme" content="light dark">
<meta name="referrer" content="no-referrer">
<title>Manifold Idler</title>
<link rel="icon" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'%3E%3Crect width='32' height='32' rx='8' fill='%232f6fde'/%3E%3Cpath d='M8 22V10l8 7 8-7v12' fill='none' stroke='white' stroke-width='3' stroke-linecap='round' stroke-linejoin='round'/%3E%3C/svg%3E">
<style nonce="{{nonce}}">
:root{--bg:#f3f4f6;--panel:#fff;--ink:#1b1f24;--muted:#5d6670;--line:#dfe3e8;--chip:#eceff3;--accent:#2f6fde;--accent-ink:#fff;--good:#1b7f45;--warn:#9a5d00;--bad:#bf3328;--focus:#2f6fde;--shadow:0 1px 2px rgba(16,24,40,.05),0 1px 3px rgba(16,24,40,.07);color-scheme:light}
@media (prefers-color-scheme:dark){:root{--bg:#111316;--panel:#1a1d21;--ink:#e6e9ed;--muted:#9aa3ad;--line:#2b3037;--chip:#23272d;--accent:#6d9dff;--accent-ink:#0b1222;--good:#52c48a;--warn:#e2a64d;--bad:#ff7466;--focus:#8db2ff;--shadow:none;color-scheme:dark}}
*{box-sizing:border-box}
[hidden]{display:none!important}
html{-webkit-text-size-adjust:100%}
body{margin:0;background:var(--bg);color:var(--ink);font:15px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,Ubuntu,Cantarell,"Noto Sans",sans-serif}
button,input,select{font:inherit;color:inherit}
:focus-visible{outline:2px solid var(--focus);outline-offset:2px}
.top{position:sticky;top:0;z-index:5;display:flex;align-items:center;justify-content:space-between;gap:12px;padding:10px 16px;background:var(--bg);border-bottom:1px solid var(--line)}
.brand{display:flex;align-items:center;gap:10px;min-width:0}
.mark{flex:none;width:28px;height:28px;display:block}
.mark rect{fill:var(--accent)}
.mark path{fill:none;stroke:var(--accent-ink);stroke-width:3;stroke-linecap:round;stroke-linejoin:round}
h1{margin:0;font-size:17px;font-weight:650;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.run{display:flex;align-items:center;gap:10px;min-width:0}
.pill{display:inline-flex;align-items:center;gap:7px;max-width:46vw;padding:4px 10px;border-radius:999px;background:var(--chip);color:var(--muted);font-size:13px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.pill::before{content:"";flex:none;width:8px;height:8px;border-radius:50%;background:currentColor}
.good{color:var(--good)}.warn{color:var(--warn)}.bad{color:var(--bad)}.muted{color:var(--muted)}
main{max-width:1040px;margin:0 auto;padding:16px;display:grid;gap:16px}
.card{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:16px;box-shadow:var(--shadow);min-width:0}
.head{display:flex;align-items:center;justify-content:space-between;gap:12px;flex-wrap:wrap;margin-bottom:12px}
h2{margin:0;font-size:15px;font-weight:650}
.card>h2{margin-bottom:12px}
.facts{display:grid;grid-template-columns:repeat(auto-fill,minmax(130px,1fr));gap:10px 16px;margin:0 0 14px}
.fact dt{font-size:12px;color:var(--muted);text-transform:uppercase;letter-spacing:.04em}
.fact dd{margin:2px 0 0;overflow-wrap:anywhere}
.inline{display:flex;align-items:center;gap:8px;flex-wrap:wrap}
.inline label{color:var(--muted)}
input,select{background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:6px 9px;min-height:34px}
input[type=number]{width:84px}
input[type=search]{flex:1 1 220px;min-width:0}
button{cursor:pointer;border:1px solid var(--line);background:var(--chip);border-radius:8px;padding:6px 12px;min-height:34px;white-space:nowrap}
button:hover:not(:disabled){border-color:var(--muted)}
button:disabled{cursor:default;opacity:.55}
button.primary{background:var(--accent);border-color:var(--accent);color:var(--accent-ink);font-weight:600}
button.small{padding:3px 10px;min-height:30px;font-size:14px}
button.armed{background:var(--bad);border-color:var(--bad);color:#fff}
button.link{border:0;background:none;padding:0;min-height:0;color:inherit;text-decoration:underline dotted;text-underline-offset:3px;text-align:left;white-space:normal}
#toggle{min-width:76px}
.list-pick{display:flex;align-items:center;gap:8px;color:var(--muted)}
.list-pick select{max-width:60vw}
table{width:100%;border-collapse:collapse}
th{text-align:left;font-size:12px;font-weight:600;color:var(--muted);text-transform:uppercase;letter-spacing:.04em;padding:0 10px 8px 0;border-bottom:1px solid var(--line)}
td{padding:9px 10px 9px 0;border-bottom:1px solid var(--line);vertical-align:middle}
tr:last-child td{border-bottom:0}
td.num,th.num{font-variant-numeric:tabular-nums;white-space:nowrap}
td.end,th.end{text-align:right;padding-right:0}
.name{display:block;font-weight:550;overflow-wrap:anywhere}
.app{display:block;font-size:12px;color:var(--muted);font-variant-numeric:tabular-nums}
.state{display:inline-flex;align-items:center;gap:6px}
.state::before{content:"";flex:none;width:7px;height:7px;border-radius:50%;background:currentColor}
.goal-edit{display:flex;gap:6px;flex-wrap:wrap;align-items:center}
.goal-edit input{width:76px}
.search{display:flex;gap:8px;flex-wrap:wrap;align-items:center;margin-bottom:12px}
.search label{display:flex;align-items:center;gap:8px;color:var(--muted)}
.quiet{color:var(--muted);margin:10px 0 0}
details summary{cursor:pointer;list-style:none;display:flex;align-items:center;gap:8px}
details summary::-webkit-details-marker{display:none}
details summary::before{content:"";width:7px;height:7px;border-right:2px solid var(--muted);border-bottom:2px solid var(--muted);transform:rotate(-45deg);transition:transform .15s}
details[open] summary::before{transform:rotate(45deg)}
.log{list-style:none;margin:12px 0 0;padding:0;max-height:420px;overflow:auto;font:12.5px/1.5 ui-monospace,SFMono-Regular,Consolas,"Liberation Mono",monospace}
.log li{display:grid;grid-template-columns:auto auto 1fr;gap:10px;padding:2px 0;border-bottom:1px dashed var(--line)}
.log .when{color:var(--muted)}
.log .level{min-width:58px}
.log .text{overflow-wrap:anywhere}
.banner{max-width:1040px;margin:16px auto 0;padding:10px 14px;border-radius:10px;border:1px solid var(--bad);color:var(--bad);background:var(--panel)}
.banner-wrap{padding:0 16px}
.toast{position:fixed;left:50%;bottom:18px;transform:translateX(-50%);max-width:min(560px,calc(100vw - 32px));padding:10px 14px;border-radius:10px;background:var(--ink);color:var(--bg);box-shadow:0 6px 24px rgba(0,0,0,.25);white-space:pre-line;opacity:0;pointer-events:none;transition:opacity .2s}
.toast.show{opacity:1}
.toast.failed{background:var(--bad);color:#fff}
.sr{position:absolute;width:1px;height:1px;overflow:hidden;clip:rect(0 0 0 0);white-space:nowrap}
@media (max-width:640px){
.top{flex-wrap:wrap}
.pill{max-width:calc(100vw - 140px)}
table.rows thead{display:none}
table.rows tr{display:grid;grid-template-columns:1fr auto;gap:4px 12px;padding:10px 0;border-bottom:1px solid var(--line)}
table.rows tr:last-child{border-bottom:0}
table.rows td{padding:0;border:0;grid-column:1/-1}
table.rows td.lead{grid-column:1;grid-row:1}
table.rows td.end{grid-column:2;grid-row:1;align-self:start}
table.rows td[data-label]::before{content:attr(data-label) " ";color:var(--muted);font-size:12px;text-transform:uppercase;letter-spacing:.04em}
}
@media (prefers-reduced-motion:reduce){*{transition:none!important}}
</style>
</head>
)PAGE";

        constexpr std::string_view kBody = R"PAGE(<body>
<header class="top">
<div class="brand"><svg class="mark" viewBox="0 0 32 32" aria-hidden="true"><rect width="32" height="32" rx="8"/><path d="M8 22V10l8 7 8-7v12"/></svg><h1>Manifold Idler</h1></div>
<div class="run"><span id="state" class="pill muted">Connecting</span><button id="toggle" class="primary" type="button" disabled>Start</button></div>
</header>
<div class="banner-wrap"><div id="banner" class="banner" role="alert" hidden></div></div>
<noscript><div class="banner-wrap"><div class="banner">This page needs JavaScript.</div></div></noscript>
<main>
<section class="card" aria-labelledby="h-status">
<h2 id="h-status">Status</h2>
<dl id="status" class="facts"></dl>
<form id="slots-form" class="inline">
<label for="slots">Games at once</label>
<input id="slots" type="number" min="1" max="32" step="1" inputmode="numeric" required>
<button type="submit">Set</button>
</form>
</section>
<section class="card" aria-labelledby="h-queue">
<div class="head">
<h2 id="h-queue">Queue</h2>
<label class="list-pick">List <select id="lists"></select></label>
</div>
<table class="rows" id="queue-table">
<thead><tr><th>Game</th><th>State</th><th>Goal</th><th class="num">Idled</th><th class="end"><span class="sr">Remove</span></th></tr></thead>
<tbody id="queue"></tbody>
</table>
<p id="queue-empty" class="quiet" hidden>The queue is empty. Games come in below.</p>
</section>
<section class="card" aria-labelledby="h-add">
<h2 id="h-add">Add games</h2>
<div class="search">
<input id="search" type="search" placeholder="Name or app id" autocomplete="off" spellcheck="false" aria-label="Find a game by name or app id">
<label>Goal <select id="add-kind">
<option value="">The usual one</option>
<option value="none">No limit</option>
<option value="for">For hours</option>
<option value="until">Until hours played</option>
</select></label>
<input id="add-hours" type="number" min="0.1" step="any" value="3" inputmode="decimal" aria-label="Hours" hidden>
</div>
<table class="rows" id="games-table">
<thead><tr><th>Game</th><th class="num">Played</th><th class="end"><span class="sr">Add</span></th></tr></thead>
<tbody id="games"></tbody>
</table>
<p id="games-note" class="quiet"></p>
</section>
<section class="card">
<details id="log-box">
<summary><h2>Log</h2></summary>
<ol id="log" class="log"></ol>
</details>
</section>
</main>
<div id="toast" class="toast" role="status" aria-live="polite"></div>
)PAGE";

        constexpr std::string_view kScriptStart = R"PAGE(<script nonce="{{nonce}}">
"use strict";
(() => {
const token = new URLSearchParams(location.search).get("token") || "";
const $ = (id) => document.getElementById(id);
const pollMilliseconds = 2000;
const retryMilliseconds = 5000;

class TokenError extends Error {}

let timer = 0;
let refreshing = false;
let again = false;
let failing = false;
let locked = false;
let active = false;
let editing = null;
let queueEntries = [];
let listSignature = "";
let searchNumber = 0;
let searchTimer = 0;
let toastTimer = 0;
const queueRows = new Map();

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
    throw new Error("The idler does not answer. It may have ended; this page keeps trying.");
  }
  if (response.status === 401) throw new TokenError();
  let data;
  try {
    data = await response.json();
  } catch (error) {
    throw new Error("The idler gave an answer that makes no sense (" + response.status + ").");
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
  for (const control of document.querySelectorAll("button, input, select")) control.disabled = true;
  setPill("No token", "bad");
  showBanner("The token is missing or wrong. The whole address, with its token, comes from: idler web");
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
)PAGE";

        constexpr std::string_view kScriptViews = R"PAGE(
function drawStatus(rows) {
  const facts = rows.filter((row) => row.label && row.key !== "slots");
  $("status").replaceChildren(...facts.map((row) => el("div", { class: "fact" }, el("dt", { text: row.label }), el("dd", { text: row.value }))));
  const find = (key) => (rows.find((row) => row.key === key) || {}).value || "";
  const state = find("state");
  setPill(state || "Unknown", tone(state));
  active = find("active") === "yes";
  const toggle = $("toggle");
  toggle.textContent = active ? "Stop" : "Start";
  toggle.classList.toggle("primary", !active);
  toggle.disabled = false;
  const slots = $("slots");
  if (document.activeElement !== slots) slots.value = find("slots");
}

function goalEditor(entry) {
  const kind = el("select", { "aria-label": "Goal for " + entry.name },
    el("option", { value: "none", text: "No limit" }),
    el("option", { value: "for", text: "For hours" }),
    el("option", { value: "until", text: "Until hours played" }));
  kind.value = entry.kind === "for" || entry.kind === "until" ? entry.kind : "none";
  const hours = el("input", { type: "number", min: "0.1", step: "any", inputmode: "decimal", value: entry.hours || "3", "aria-label": "Hours" });
  const fit = () => { hours.hidden = kind.value === "none"; };
  kind.addEventListener("change", fit);
  fit();
  const close = () => { editing = null; drawQueue(); };
  const form = el("form", { class: "goal-edit" }, kind, hours,
    el("button", { type: "submit", class: "small primary", text: "Save" }),
    el("button", { type: "button", class: "small", text: "Cancel", onclick: close }));
  form.addEventListener("submit", async (event) => {
    event.preventDefault();
    const words = ["goal", entry.app, kind.value];
    if (kind.value !== "none") words.push(hours.value);
    const result = await act(words, true);
    if (result.ok) {
      close();
      refresh();
    }
  });
  form.addEventListener("keydown", (event) => { if (event.key === "Escape") close(); });
  setTimeout(() => kind.focus(), 0);
  return form;
}

function goalCell(row) {
  const entry = row.entry;
  if (editing === entry.app) return goalEditor(entry);
  return el("button", { type: "button", class: "link", text: entry.goal, title: "Change the goal", "aria-label": "Goal of " + entry.name + ": " + entry.goal + ". Change it", onclick: () => { editing = entry.app; drawQueue(); } });
}

function removeButton(row) {
  const button = el("button", { type: "button", class: "small", text: "Remove" });
  let armed = 0;
  const disarm = () => {
    clearTimeout(armed);
    armed = 0;
    button.textContent = "Remove";
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
    await act(["remove", row.entry.app], true);
    button.disabled = false;
    refresh();
    search(false);
  });
  button.addEventListener("blur", () => { if (armed) disarm(); });
  return button;
}

function makeQueueRow(entry) {
  const row = { entry, editing: false };
  row.name = el("span", { class: "name" });
  row.app = el("span", { class: "app" });
  row.state = el("span", { class: "state" });
  row.goal = el("td", { "data-label": "Goal" });
  row.idled = el("td", { class: "num", "data-label": "Idled" });
  row.tr = el("tr", {},
    el("td", { class: "lead" }, row.name, row.app),
    el("td", { "data-label": "State" }, row.state),
    row.goal,
    row.idled,
    el("td", { class: "end" }, removeButton(row)));
  return row;
}

function setText(node, text) {
  if (node.textContent !== text) node.textContent = text;
}

function fillQueueRow(row, entry) {
  const goalChanged = row.entry.goal !== entry.goal || row.entry.kind !== entry.kind || row.entry.hours !== entry.hours;
  const editingNow = editing === entry.app;
  row.entry = entry;
  setText(row.name, entry.name);
  setText(row.app, entry.app);
  setText(row.state, entry.state);
  row.state.className = "state " + tone(entry.state);
  setText(row.idled, entry.idled);
  if (!row.goal.firstChild || row.editing !== editingNow || (!editingNow && goalChanged)) {
    row.goal.replaceChildren(goalCell(row));
    row.editing = editingNow;
  }
}

function drawQueue() {
  const body = $("queue");
  const seen = new Set();
  queueEntries.forEach((entry, index) => {
    seen.add(entry.app);
    let row = queueRows.get(entry.app);
    if (!row) {
      row = makeQueueRow(entry);
      queueRows.set(entry.app, row);
    }
    fillQueueRow(row, entry);
    if (body.children[index] !== row.tr) body.insertBefore(row.tr, body.children[index] || null);
  });
  for (const [app, row] of queueRows) {
    if (seen.has(app)) continue;
    row.tr.remove();
    queueRows.delete(app);
    if (editing === app) editing = null;
  }
  $("queue-table").hidden = queueEntries.length === 0;
  $("queue-empty").hidden = queueEntries.length !== 0;
}

function drawLists(rows) {
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
}
)PAGE";

        constexpr std::string_view kScriptEnd = R"PAGE(
function goalWords() {
  const kind = $("add-kind").value;
  if (!kind) return [];
  if (kind === "none") return ["none"];
  return [kind, $("add-hours").value];
}

function gameRow(game) {
  const button = el("button", { type: "button", class: "small", text: game.queued === "yes" ? "In queue" : "Add" });
  button.disabled = game.queued === "yes";
  button.addEventListener("click", async () => {
    button.disabled = true;
    const result = await act(["add", ...goalWords(), "--", game.app]);
    if (result.ok) button.textContent = "In queue";
    else button.disabled = false;
    refresh();
  });
  return el("tr", {},
    el("td", { class: "lead" }, el("span", { class: "name", text: game.name }), el("span", { class: "app", text: game.app })),
    el("td", { class: "num", "data-label": "Played", text: game.playtime }),
    el("td", { class: "end" }, button));
}

async function search(soon) {
  clearTimeout(searchTimer);
  if (soon) {
    searchTimer = setTimeout(() => search(false), 250);
    return;
  }
  if (locked) return;
  const number = ++searchNumber;
  let result;
  try {
    result = await call(["games", "100", $("search").value.trim()]);
  } catch (error) {
    trouble(error);
    return;
  }
  if (number !== searchNumber) return;
  const lines = result.text.split("\n");
  $("games-note").textContent = result.ok ? lines[lines.length - 1] : result.text;
  $("games").replaceChildren(...(result.ok ? result.rows.map(gameRow) : []));
  $("games-table").hidden = !result.ok || result.rows.length === 0;
  if (!result.ok) searchTimer = setTimeout(() => search(false), retryMilliseconds);
}

async function loadLog() {
  const list = $("log");
  const atEnd = list.scrollTop + list.clientHeight >= list.scrollHeight - 8;
  const result = await call(["log", "200"]);
  const level = (name) => name === "ERROR" ? "bad" : name === "WARN" ? "warn" : name === "SUCCESS" ? "good" : "muted";
  list.replaceChildren(...result.rows.map((row) => el("li", {},
    el("span", { class: "when", text: row.time }),
    el("span", { class: "level " + level(row.level), text: row.level }),
    el("span", { class: "text", text: row.message }))));
  if (result.rows.length === 0) list.append(el("li", {}, el("span", { class: "text muted", text: "Nothing yet." })));
  if (atEnd) list.scrollTop = list.scrollHeight;
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
    const [status, queue, lists] = await Promise.all([call(["status"]), call(["queue"]), call(["lists"])]);
    const wasFailing = failing;
    failing = false;
    hideBanner();
    drawStatus(status.rows);
    queueEntries = queue.rows;
    drawQueue();
    drawLists(lists.rows);
    if ($("log-box").open) await loadLog();
    if (wasFailing) search(false);
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

$("toggle").addEventListener("click", async () => {
  const button = $("toggle");
  button.disabled = true;
  await act([active ? "stop" : "start"]);
  refresh();
});

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
  refresh();
  search(false);
});

$("search").addEventListener("input", () => search(true));
$("add-kind").addEventListener("change", () => {
  const kind = $("add-kind").value;
  $("add-hours").hidden = kind === "" || kind === "none";
});
$("log-box").addEventListener("toggle", () => { if ($("log-box").open) loadLog().catch(trouble); });
document.addEventListener("visibilitychange", () => {
  if (document.hidden) clearTimeout(timer);
  else refresh();
});

if (!token) {
  lockPage();
} else {
  refresh();
  search(false);
}
})();
</script>
</body>
</html>
)PAGE";

        constexpr std::array<std::string_view, 5> kParts{ kHead, kBody, kScriptStart, kScriptViews, kScriptEnd };
        constexpr std::string_view kNoncePlace = "{{nonce}}";
    }

    std::string WebPage(std::string_view nonce)
    {
        std::string page;
        for (const std::string_view part : kParts)
            page += part;
        for (std::size_t at = page.find(kNoncePlace); at != std::string::npos; at = page.find(kNoncePlace, at + nonce.size()))
            page.replace(at, kNoncePlace.size(), nonce);
        return page;
    }
}

<script setup lang="ts">
import { ref, computed, onMounted, onUnmounted, watch } from "vue";
import TracePlot from "./TracePlot.vue";
type Trace = {
  header: Record<string, number | number[] | number[][]>;
  cobo: number;
  asad: number;
  aget: number;
  channel: number;
  samples: (number | null)[];
  path: string;
  frame: number;
  offset: number;
  timestamp: number;
};
const runs = ref<string[]>([]),
  run = ref(""),
  events = ref<number[]>([]),
  position = ref(0),
  traces = ref<Trace[]>([]),
  selected = ref(0),
  cobo = ref(0),
  error = ref(""),
  busy = ref(false);
let generation = 0;
const trace = computed(() => traces.value[selected.value]);
const cobos = computed(() => [...new Set(traces.value.map((t) => t.cobo))]);
const cells = computed(() =>
  Array.from({ length: 1088 }, (_, i) => ({
    row: Math.floor(i / 68),
    ch: i % 68,
    index: traces.value.findIndex(
      (t) =>
        t.cobo === cobo.value &&
        t.asad === Math.floor(i / 272) &&
        t.aget === Math.floor(i / 68) % 4 &&
        t.channel === i % 68,
    ),
  })),
);
const eventInput = ref<number | string>("");
const eventError = ref("");
watch(
  () => events.value[position.value],
  (id) => {
    eventInput.value = id ?? "";
    eventError.value = "";
  },
);
function applyEvent() {
  const id = Number(eventInput.value);
  const index = events.value.indexOf(id);
  if (eventInput.value === "" || !Number.isInteger(id) || index < 0) {
    eventError.value = "Event ID not found";
    return;
  }
  eventError.value = "";
  if (index !== position.value) {
    position.value = index;
    void loadEvent();
  }
}
async function get(url: string) {
  const r = await fetch("/api/" + url);
  if (!r.ok) throw Error((await r.json()).detail);
  return r.json();
}
async function loadRun() {
  const g = ++generation;
  busy.value = true;
  error.value = "";
  traces.value = [];
  try {
    const e = await get(`runs/${encodeURIComponent(run.value)}/events`);
    if (g !== generation) return;
    events.value = e;
    position.value = 0;
    await loadEvent();
  } catch (e) {
    error.value = String(e);
  } finally {
    if (g === generation) busy.value = false;
  }
}
async function loadEvent() {
  const g = ++generation;
  busy.value = true;
  error.value = "";
  const old = trace.value;
  try {
    if (!events.value.length) {
      traces.value = [];
      return;
    }
    const ts = await get(
      `runs/${encodeURIComponent(run.value)}/events/${events.value[position.value]}`,
    );
    if (g !== generation) return;
    traces.value = ts;
    selected.value = Math.max(
      0,
      ts.findIndex(
        (t: Trace) =>
          old &&
          t.cobo === old.cobo &&
          t.asad === old.asad &&
          t.aget === old.aget &&
          t.channel === old.channel,
      ),
    );
    cobo.value = trace.value?.cobo ?? 0;
  } catch (e) {
    if (g === generation) {
      error.value = String(e);
      traces.value = [];
    }
  } finally {
    if (g === generation) busy.value = false;
  }
}
function move(n: number) {
  const p = Math.max(0, Math.min(events.value.length - 1, position.value + n));
  if (p !== position.value) {
    position.value = p;
    void loadEvent();
  }
}
function key(e: KeyboardEvent) {
  if (
    (e.target as HTMLElement).closest("input,select,textarea,[contenteditable]") ||
    e.ctrlKey ||
    e.altKey ||
    e.metaKey
  )
    return;
  if (!"hjkl".includes(e.key)) return;
  e.preventDefault();
  if (e.key === "h" || e.key === "l") move(e.key === "h" ? -1 : 1);
  else {
    selected.value = Math.max(
      0,
      Math.min(traces.value.length - 1, selected.value + (e.key === "j" ? 1 : -1)),
    );
    cobo.value = trace.value?.cobo ?? 0;
  }
}
onMounted(async () => {
  window.addEventListener("keydown", key);
  try {
    runs.value = await get("runs");
  } catch (e) {
    error.value = String(e);
  }
});
onUnmounted(() => window.removeEventListener("keydown", key));
</script>
<template>
  <main>
    <header>
      <small>AGET / RAW DATA</small>
      <h1>CoBo trace browser</h1>
      <p>Explore acquisition events and their raw waveforms.</p>
    </header>
    <section class="controls">
      <label
        >Run
        <select v-model="run" @change="loadRun">
          <option disabled value="">Select a run</option>
          <option v-for="r in runs">{{ r }}</option>
        </select></label
      ><label
        >Event
        <input
          v-model="eventInput"
          type="number"
          step="1"
          :disabled="!events.length"
          :aria-invalid="!!eventError"
          aria-describedby="event-error"
          @change="applyEvent"
          @keydown.enter="applyEvent"
        /> </label
      ><span id="event-error" role="alert" v-if="eventError">{{ eventError }}</span
      ><span>{{ busy ? "Reading…" : `${events.length} events` }}</span>
    </section>
    <p role="alert" v-if="error">{{ error }}</p>
    <div class="workspace">
      <aside class="metadata">
        <div v-if="trace" class="frame-header">
          <h2>Frame meta</h2>
          <p>
            Revision {{ trace.header.revision }} · sizes in 256-byte blocks · hit patterns shown as
            wire bytes
          </p>
          <dl>
            <dt>Source path</dt>
            <dd>{{ trace.path }}</dd>
            <dt>Frame ordinal</dt>
            <dd>{{ trace.frame }}</dd>
            <dt>Byte offset</dt>
            <dd>{{ trace.offset }}</dd>
            <dt>Timestamp</dt>
            <dd>{{ trace.timestamp }}</dd>
            <template v-for="(value, name) in trace.header" :key="name">
              <dt>{{ String(name).replaceAll("_", " ") }}</dt>
              <dd v-if="name === 'hit_patterns'">
                <div v-for="(pattern, aget) in value as number[][]" :key="aget">
                  AGET {{ aget }}:
                  {{ pattern.map((byte) => byte.toString(16).padStart(2, "0")).join(" ") }}
                </div>
              </dd>
              <dd v-else-if="name === 'reserved'">
                <details>
                  <summary>Reserved bytes</summary>
                  {{ value }}
                </details>
              </dd>
              <dd v-else>{{ Array.isArray(value) ? value.join(", ") : value }}</dd>
            </template>
          </dl>
        </div>

        <p v-if="!trace">Select a run and channel to inspect frame metadata.</p>
      </aside>
      <div class="viewer">
        <section>
          <div class="heading">
            <h2>Channel map</h2>
            <label
              >CoBo
              <select v-model="cobo">
                <option v-for="c in cobos">{{ c }}</option>
              </select></label
            ><span>Green: recorded · Purple: FPN · j / k: next / previous trace</span>
          </div>
          <div class="scroll">
            <div class="map">
              <button
                v-for="cell in cells"
                :key="cell.row * 68 + cell.ch"
                :disabled="cell.index < 0"
                :title="`AsAd ${Math.floor(cell.row / 4)} AGET ${cell.row % 4} Channel ${cell.ch}`"
                :aria-label="`AsAd ${Math.floor(cell.row / 4)} AGET ${cell.row % 4} Channel ${cell.ch}`"
                :class="{
                  hit: cell.index >= 0,
                  fpn: [11, 22, 45, 56].includes(cell.ch),
                  selected: cell.index === selected && cell.index >= 0,
                }"
                :style="{
                  marginTop: cell.row % 4 === 0 ? '8px' : '0',
                  marginRight: cell.ch % 8 === 7 ? '3px' : '0',
                }"
                @click="selected = cell.index"
              />
            </div>
          </div>
        </section>
        <section class="waveform">
          <h2>
            {{
              trace
                ? `CoBo ${trace.cobo} / AsAd ${trace.asad} / AGET ${trace.aget} / Channel ${trace.channel}`
                : "Select a run to begin"
            }}
          </h2>
          <TracePlot
            v-if="trace"
            :samples="trace.samples"
            :identity="run + ':' + events[position] + ':' + selected"
          />
          <p v-else>Select a recorded channel to view its waveform.</p>
        </section>
      </div>
    </div>
  </main>
</template>
<style>
body {
  margin: 0;
  background: #f3eddf;
  color: #241f17;
  font:
    15px "Segoe UI",
    sans-serif;
}
main {
  max-width: 1920px;
  margin: auto;
  padding: 16px;
}
small {
  color: #205845;
  letter-spacing: 0.15em;
}
h1 {
  font-size: 24px;
  margin: 8px 0;
}
h2 {
  font-size: 18px;
}
p {
  color: #756b5a;
}
section {
  background: #fffcf6;
  border: 1px solid #ded5c4;
  border-radius: 14px;
  padding: 12px;
  margin-top: 12px;
}
.controls,
.heading {
  display: flex;
  gap: 16px;
  align-items: center;
  flex-wrap: wrap;
}
button,
input,
select {
  padding: 10px;
  border: 1px solid #c6beae;
  border-radius: 6px;
  background: #fffdf8;
  color: inherit;
}
button {
  cursor: pointer;
}
button:focus-visible,
select:focus-visible,
input:focus-visible {
  outline: 3px solid #b57c25;
}
.scroll {
  overflow: auto;
  padding: 5px;
}
.map {
  display: grid;
  grid-template-columns: repeat(68, 1fr);
  gap: 3px;
  min-width: 950px;
}
.map button {
  padding: 0;
  border-radius: 2px;
  height: 14px;
  background: #e6e0d4;
  border: 1px solid transparent;
}
.map .hit {
  background: #43856a;
}
.map .fpn {
  border: 2px solid #9577b0;
}
.map .selected {
  outline: 2px solid #d69629;
  background: #e4b75c;
}
.frame-header dl {
  display: grid;
  grid-template-columns: minmax(95px, 110px) minmax(0, 1fr);
  gap: 6px 10px;
}
.frame-header dt {
  text-transform: capitalize;
  color: #756b5a;
}
.frame-header dd {
  margin: 0;
  overflow-wrap: anywhere;
  font-family: monospace;
}
span {
  font-size: 13px;
  color: #756b5a;
}
@media (max-width: 650px) {
  main {
    padding: 12px;
  }
  section {
    padding: 12px;
  }
}

* {
  box-sizing: border-box;
}
header p {
  margin: 4px 0;
}
.controls input {
  width: 110px;
}
.controls label {
  display: flex;
  align-items: center;
  gap: 8px;
}
#event-error {
  color: #9c3025;
}
.workspace {
  display: grid;
  grid-template-columns: 320px minmax(0, 1fr);
  gap: 12px;
  margin-top: 12px;
}
.metadata {
  background: #fffcf6;
  border: 1px solid #ded5c4;
  border-radius: 14px;
  padding: 12px;
  overflow: auto;
}
.metadata h2,
.viewer h2 {
  margin: 0 0 8px;
}
.metadata p {
  font-size: 12px;
}
.frame-header dl {
  font-size: 12px;
}
.viewer {
  min-width: 0;
  display: flex;
  flex-direction: column;
  gap: 12px;
}
.viewer section {
  margin: 0;
  min-width: 0;
}
.heading {
  gap: 10px;
}
.heading span {
  flex: 1;
}
.waveform {
  flex: 1;
  min-height: 290px;
  display: flex;
  flex-direction: column;
}
.waveform h2 {
  font-size: 16px;
}
.trace-plot {
  flex: 1;
}
@media (min-width: 1200px) {
  main {
    height: 100dvh;
    display: flex;
    flex-direction: column;
  }
  .workspace {
    flex: 1;
    min-height: 0;
  }
  .viewer {
    min-height: 0;
  }
  .waveform {
    min-height: 0;
  }
}
@media (max-width: 1199px) {
  .workspace {
    grid-template-columns: 1fr;
  }
  .metadata {
    order: 2;
    max-height: 420px;
  }
  .waveform {
    height: 360px;
    flex: auto;
  }
}

@media (min-width: 1200px) {
  .map {
    gap: 2px;
  }
  .map button {
    height: 12px;
  }
  .waveform .trace-plot {
    height: 0;
    min-height: 0;
  }
}
</style>

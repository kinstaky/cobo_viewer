<script setup lang="ts">
import { ref, watch, onMounted, onBeforeUnmount } from "vue";
import Plotly from "plotly.js/lib/core";
import scatter from "plotly.js/lib/scatter";

Plotly.register([scatter]);
const props = defineProps<{ samples: (number | null)[]; identity: string }>();
const root = ref<HTMLDivElement>();
let observer: ResizeObserver;
function render() {
  if (!root.value) return;
  void Plotly.react(
    root.value,
    [
      {
        type: "scatter",
        mode: "lines",
        x: props.samples.map((_, i) => i),
        y: props.samples,
        connectgaps: false,
        line: { color: "#205845", width: 2 },
        hovertemplate: "Time bucket: %{x}<br>ADC: %{y}<extra></extra>",
      },
    ],
    {
      margin: { t: 16, r: 20, b: 52, l: 62 },
      paper_bgcolor: "#fffcf6",
      plot_bgcolor: "#fffcf6",
      font: { family: "Segoe UI, sans-serif", color: "#241f17" },
      xaxis: {
        title: { text: "Time bucket" },
        range: [0, 511],
        gridcolor: "#e6e0d4",
        zeroline: false,
      },
      yaxis: { title: { text: "ADC" }, range: [0, 4095], gridcolor: "#e6e0d4", zeroline: false },
      showlegend: false,
      hovermode: "closest",
      autosize: true,
    },
    { displayModeBar: false, responsive: true, doubleClick: "reset" },
  );
}
onMounted(() => {
  render();
  observer = new ResizeObserver(() => {
    if (root.value) void Plotly.Plots.resize(root.value);
  });
  observer.observe(root.value!);
});
watch(() => [props.samples, props.identity], render);
onBeforeUnmount(() => {
  observer?.disconnect();
  if (root.value) Plotly.purge(root.value);
});
</script>
<template>
  <div ref="root" class="trace-plot" role="img" aria-label="Raw waveform: time bucket versus ADC" />
</template>
<style scoped>
.trace-plot {
  width: 100%;
  height: 100%;
  min-height: 220px;
}
</style>

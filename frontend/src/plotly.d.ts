declare module "plotly.js/lib/core" {
  const Plotly: {
    register(modules: unknown[]): void;
    react(root: HTMLElement, data: unknown[], layout: unknown, config: unknown): Promise<void>;
    purge(root: HTMLElement): void;
    Plots: { resize(root: HTMLElement): Promise<void> };
  };
  export default Plotly;
}
declare module "plotly.js/lib/scatter" {
  const scatter: unknown;
  export default scatter;
}

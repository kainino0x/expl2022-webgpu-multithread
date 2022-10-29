let device;
let pipeline;

globalThis.onmessage = ev => {
  if (ev.data !== 'unavailable') {
    const { table, dispatchCountPerWorker } = ev.data;
    if (table) {
      device = table.get(1);
      pipeline = device.createComputePipeline({
        layout: 'auto',
        compute: {
          module: device.createShaderModule({
            code: `@compute @workgroup_size(64) fn main() {}`,
          }),
          entryPoint: 'main',
        },
      });
      return;
    }

    (function bench() {
      const s = performance.now();
      const encoder = device.createCommandEncoder();
      const pass = encoder.beginComputePass();
      pass.setPipeline(pipeline);
      for (let i = 0; i < dispatchCountPerWorker; ++i) {
        pass.dispatchWorkgroups(1, 2, 3);
      }
      pass.end();
      encoder.finish();
      const time = performance.now() - s;
      postMessage(time);
    })();
  }
}

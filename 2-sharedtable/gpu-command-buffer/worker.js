class WorkerRenderer {
  constructor(idx, width, height, table) {
    this.idx = idx;
    this.table = table;
    this.device = table.get(1);
    this.pipeline = table.get(2);
    this.positionBuffer = table.get(20 + idx);
    this.colorBuffer = table.get(4);
    this.indexBuffer = table.get(5);

    this.width = width;
    this.height = height;
  }

  encodeCommands() {
    const encoder = this.device.createRenderBundleEncoder({
      colorFormats: ['bgra8unorm'],
    });
    encoder.setPipeline(this.pipeline);
    encoder.setVertexBuffer(0, this.positionBuffer);
    encoder.setVertexBuffer(1, this.colorBuffer);
    encoder.setIndexBuffer(this.indexBuffer, 'uint16');
    encoder.drawIndexed(3, 1);
    const renderBundle = encoder.finish();

    this.table.insert(10 + this.idx, renderBundle);
  }
}

let renderer;

globalThis.onmessage = ev => {
  if (ev.data !== 'unavailable') {
    if (ev.data === 'frame') {
      renderer.encodeCommands();
      postMessage('done');
    } else {
      // initialize
      const { idx, width, height, table } = ev.data;
      renderer = new WorkerRenderer(idx, width, height, table);

      console.log('worker initialize');
      console.log(table);
    }
  }

  postMessage('done');
}

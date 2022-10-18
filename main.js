const worker = new Worker('./worker.js');

const adapter = await navigator.gpu.requestAdapter();
const device = await adapter.requestDevice();

const buffer = device.createBuffer({ size: 8, usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ });

worker.onmessage = async () => {
  await buffer.mapAsync(GPUMapMode.READ);
  const mapped = buffer.getMappedRange();
  console.log(new Uint32Array(mapped));
};
try {
  worker.postMessage({ device, buffer });
} catch (ex) {
  console.log('postMessage with WebGPU objects failed');
  worker.postMessage(null);
} finally {
  device.queue.writeBuffer(buffer, 4, new Uint32Array([77777]));
}



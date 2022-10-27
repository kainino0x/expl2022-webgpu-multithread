const worker = new Worker('./worker.js');

const adapter = await navigator.gpu.requestAdapter();
const device = await adapter.requestDevice();

const buffer = device.createBuffer({ size: 8, usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ });

const table = new GPUSharedTable();
table.insert(1, device);
table.insert(2, buffer);
const sab = new SharedArrayBuffer();

console.log('sending', table, sab);

worker.onmessage = async () => {
  await buffer.mapAsync(GPUMapMode.READ);
  const mapped = buffer.getMappedRange();
  console.log(new Uint32Array(mapped));
};

worker.postMessage({ table, sab });
device.queue.writeBuffer(buffer, 4, new Uint32Array([77777]));



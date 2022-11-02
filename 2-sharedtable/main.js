const worker = new Worker('./worker.js');

const adapter = await navigator.gpu.requestAdapter();
const device = await adapter.requestDevice();

const buffer = device.createBuffer({ size: 8, usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ });

const table = new GPUSharedTable();
console.log('inserting into table', device, buffer);
const deviceId = table.insert(device);
const bufferId = table.insert(buffer);

console.log('sending', table, deviceId, bufferId);

worker.onmessage = async () => {
  await buffer.mapAsync(GPUMapMode.READ);
  const mapped = buffer.getMappedRange();
  console.log('read back', Array.from(new Uint32Array(mapped)));
};

worker.postMessage({ table, deviceId, bufferId });
device.queue.writeBuffer(buffer, 4, new Uint32Array([77777]));



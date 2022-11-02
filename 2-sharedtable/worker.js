globalThis.onmessage = ev => {
  console.log('received', ev.data);
  if (ev.data !== 'unavailable') {
    const { table, deviceId, bufferId } = ev.data;
    console.log('received', table, deviceId, bufferId);

    const device = table.get(deviceId);
    const buffer = table.get(bufferId);
    console.log('retrieved from table', device, buffer);

    device.queue.writeBuffer(buffer, 0, new Uint32Array([33333]));
  }

  postMessage('done');
}

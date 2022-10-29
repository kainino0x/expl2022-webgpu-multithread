globalThis.onmessage = ev => {
  console.log('received', ev.data);
  if (ev.data !== 'unavailable') {
    const { table } = ev.data;
    console.log('received', table);

    const device = table.get(1);
    const buffer = table.get(2);
    console.log('retrieved from table', device, buffer);

    device.queue.writeBuffer(buffer, 0, new Uint32Array([33333]));
  }

  postMessage('done');
}

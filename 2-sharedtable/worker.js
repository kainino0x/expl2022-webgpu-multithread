globalThis.onmessage = ev => {
  console.log('received', ev.data);
  if (ev.data !== 'unavailable') {
    const { table, sab } = ev.data;
    console.log('received', table, sab);

    const device = table.get(1);
    const buffer = table.get(2);

    device.queue.writeBuffer(buffer, 0, new Uint32Array([33333]));
  }

  postMessage('done');
}

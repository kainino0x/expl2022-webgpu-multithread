globalThis.onmessage = ev => {
  console.log('received', ev.data);
  if (ev.data !== 'unavailable') {
    const { device, buffer } = ev.data;
    console.log('received', device, buffer, buffer.size);

    device.queue.writeBuffer(buffer, 0, new Uint32Array([33333]));
  }

  postMessage('done');
}

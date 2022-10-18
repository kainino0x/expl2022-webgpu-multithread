globalThis.onmessage = ev => {
  if (ev.data) {
    const { device, buffer } = ev.data;
    console.log(device, buffer);

    device.queue.writeBuffer(buffer, 0, new Uint32Array([33333]));
  }

  postMessage('done');
}

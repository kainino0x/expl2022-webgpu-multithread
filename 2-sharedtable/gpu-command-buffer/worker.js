class WorkerRenderer {
  constructor(width, height, table) {
    this.table = table;
    this.device = table.get(1);
    this.pipeline = table.get(2);
    this.positionBuffer = table.get(3);
    this.colorBuffer = table.get(4);
    this.indexBuffer = table.get(5);

    this.width = width;
    this.height = height;
  }

  encodeCommands(colorTextureView) {
    let colorAttachment = {
        view: colorTextureView,
        clearValue: { r: 0, g: 0, b: 0, a: 1 },
        loadOp: 'clear',
        // loadOp: 'load',
        storeOp: 'store'
    };  
  
    const renderPassDesc = {
        colorAttachments: [colorAttachment],
    };
  
    const commandEncoder = this.device.createCommandEncoder();
    // const commandEncoder = this.table.get(20);
  
    // 🖌️ Encode drawing commands
    const passEncoder = commandEncoder.beginRenderPass(renderPassDesc);
    passEncoder.setPipeline(this.pipeline);
    passEncoder.setViewport(
        0,
        0,
        this.width,
        this.height,
        0,
        1
    );
    passEncoder.setScissorRect(
        0,
        0,
        this.width,
        this.height
    );
    passEncoder.setVertexBuffer(0, this.positionBuffer);
    passEncoder.setVertexBuffer(1, this.colorBuffer);
    passEncoder.setIndexBuffer(this.indexBuffer, 'uint16');
    passEncoder.drawIndexed(3, 1);
    passEncoder.end();
  
    const commandBuffer = commandEncoder.finish();
    // console.log(commandBuffer);

    // console.log('worker encode commands');

    // this.device.queue.submit([commandEncoder.finish()]);

    // return commandEncoder.finish();

    // this.table.remove(10);
    this.table.insert(10, commandBuffer);



    // // hack to avoid texture destroy?
    // colorAttachment.view = null;
  }
}

let renderer;

globalThis.onmessage = ev => {
  // console.log('received', ev.data);

  if (ev.data !== 'unavailable') {
    if (ev.data === 'frame') {
      // console.log('worker frame');
      // console.log(renderer.table.get(6));
      
      renderer.encodeCommands(renderer.table.get(6));

      postMessage('done');
    } else {
      // initialize
      // const table = ev.data;
      const { width, height, table } = ev.data;
      renderer = new WorkerRenderer(width, height, table);

      console.log('worker initialize');
      console.log(table);
    }
  }

  postMessage('done');
}

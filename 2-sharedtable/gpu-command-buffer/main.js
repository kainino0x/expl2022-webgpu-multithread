// Creadit: many code based on
// https://alain.xyz/blog/raw-webgpu

const kNumWorkers = 2;

// 🟦 Shaders
const vertWgsl = `
struct VSOut {
    @builtin(position) Position: vec4<f32>,
    @location(0) color: vec3<f32>,
 };

@vertex
fn main(@location(0) inPos: vec3<f32>,
        @location(1) inColor: vec3<f32>) -> VSOut {
    var vsOut: VSOut;
    vsOut.Position = vec4<f32>(inPos, 1.0);
    vsOut.color = inColor;
    return vsOut;
}`;

const fragWgsl = `
@fragment
fn main(@location(0) inColor: vec3<f32>) -> @location(0) vec4<f32> {
    return vec4<f32>(inColor, 1.0);
}
`;

// 🌅 Renderer
// 📈 Position Vertex Buffer Data
const positions = [
  new Float32Array([
    0.3 - 0.5, -0.3, 0.0,
   -0.3 - 0.5, -0.3, 0.0,
    0.0 - 0.5,  0.3, 0.0
  ]),
  new Float32Array([
    0.3 + 0.5, -0.3, 0.0,
   -0.3 + 0.5, -0.3, 0.0,
    0.0 + 0.5,  0.3, 0.0
  ]),
];

// 🎨 Color Vertex Buffer Data
const colors = new Float32Array([
    1.0, 0.0, 0.0, // 🔴
    0.0, 1.0, 0.0, // 🟢
    0.0, 0.0, 1.0  // 🔵
]);

// 📇 Index Buffer Data
const indices = new Uint16Array([ 0, 1, 2 ]);

class Renderer {
    constructor(canvas) {
        this.canvas = canvas;

        this.workers = [];

        this.numFinishedWorker = 0;
    }

    // 🏎️ Start the rendering engine
    async start() {
        if (await this.initializeAPI()) {
            this.resizeBackings();
            await this.initializeResources();
            this.render();
        }
        else {
            canvas.style.display = "none";
            document.getElementById("error").innerHTML = `
<p>Doesn't look like your browser supports WebGPU.</p>
<p>Try using any chromium browser's canary build and go to <code>about:flags</code> to <code>enable-unsafe-webgpu</code>.</p>`
        }
    }

    // 🌟 Initialize WebGPU
    async initializeAPI() {
        try {
            // 🏭 Entry to WebGPU
            const entry = navigator.gpu;
            if (!entry) {
                return false;
            }

            // 🔌 Physical Device Adapter
            this.adapter = await entry.requestAdapter();

            // 💻 Logical Device
            this.device = await this.adapter.requestDevice();

            // 📦 Queue
            this.queue = this.device.queue;
        } catch (e) {
            console.error(e);
            return false;
        }

        return true;
    }

    // 🍱 Initialize resources to render triangle (buffers, shaders, pipeline)
    async initializeResources() {
        // 🔺 Buffers
        // let createBuffer = (arr: Float32Array | Uint16Array, usage: number) => {
        let createBuffer = (arr, usage) => {
            // 📏 Align to 4 bytes (thanks @chrimsonite)
            let desc = {
                size: (arr.byteLength + 3) & ~3,
                usage,
                mappedAtCreation: true
            };
            let buffer = this.device.createBuffer(desc);
            const writeArray =
                arr instanceof Uint16Array
                    ? new Uint16Array(buffer.getMappedRange())
                    : new Float32Array(buffer.getMappedRange());
            writeArray.set(arr);
            buffer.unmap();
            return buffer;
        };

        this.positionBuffers = [
          createBuffer(positions[0], GPUBufferUsage.VERTEX),
          createBuffer(positions[1], GPUBufferUsage.VERTEX),
        ];
        this.colorBuffer = createBuffer(colors, GPUBufferUsage.VERTEX);
        this.indexBuffer = createBuffer(indices, GPUBufferUsage.INDEX);

        // 🖍️ Shaders
        const vsmDesc = {
            code: vertWgsl
        };
        this.vertModule = this.device.createShaderModule(vsmDesc);

        const fsmDesc = {
            code: fragWgsl
        };
        this.fragModule = this.device.createShaderModule(fsmDesc);

        // ⚗️ Graphics Pipeline

        // 🔣 Input Assembly
        const positionAttribDesc = {
            shaderLocation: 0, // [[attribute(0)]]
            offset: 0,
            format: 'float32x3'
        };
        const colorAttribDesc = {
            shaderLocation: 1, // [[attribute(1)]]
            offset: 0,
            format: 'float32x3'
        };
        const positionBufferDesc = {
            attributes: [positionAttribDesc],
            arrayStride: 4 * 3, // sizeof(float) * 3
            stepMode: 'vertex'
        };
        const colorBufferDesc = {
            attributes: [colorAttribDesc],
            arrayStride: 4 * 3, // sizeof(float) * 3
            stepMode: 'vertex'
        };

        // 🦄 Uniform Data
        const pipelineLayoutDesc = { bindGroupLayouts: [] };
        const layout = this.device.createPipelineLayout(pipelineLayoutDesc);

        // 🎭 Shader Stages
        const vertex = {
            module: this.vertModule,
            entryPoint: 'main',
            buffers: [positionBufferDesc, colorBufferDesc]
        };

        // 🌀 Color/Blend State
        const colorState = {
            format: 'bgra8unorm',
            writeMask: GPUColorWrite.ALL
        };

        const fragment = {
            module: this.fragModule,
            entryPoint: 'main',
            targets: [colorState]
        };

        // 🟨 Rasterization
        const primitive = {
            frontFace: 'cw',
            cullMode: 'none',
            topology: 'triangle-list'
        };

        const pipelineDesc = {
            layout,

            vertex,
            fragment,

            primitive,
        };
        this.pipeline = this.device.createRenderPipeline(pipelineDesc);


        // worker setup
        const table = new GPUSharedTable();
        table.insert(1, this.device);
        table.insert(2, this.pipeline);
        table.insert(20, this.positionBuffers[0]);
        table.insert(21, this.positionBuffers[1]);
        table.insert(4, this.colorBuffer);
        table.insert(5, this.indexBuffer);
        this.table = table;

        for (let i = 0; i < kNumWorkers; i++) {
          const worker = new Worker('./worker.js');
          worker.postMessage({
            idx: i,
            width: this.canvas.width,
            height: this.canvas.height,
            table: this.table
          });

          worker.onmessage = async () => {
            this.numFinishedWorker++;
          };

          this.workers.push(worker);
        }
    }


    // ↙️ Resize Canvas, frame buffer attachments
    resizeBackings() {
        // ⛓️ Canvas Context
        if (!this.context) {
            this.context = this.canvas.getContext('webgpu');
            const canvasConfig = {
                device: this.device,
                alphaMode: "opaque",
                format: 'bgra8unorm',
                usage:
                    GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.COPY_SRC
            };
            this.context.configure(canvasConfig);
        }
    }


    render = () => {
        // ⏭ Acquire next image from context
        this.colorTexture = this.context.getCurrentTexture();
        this.colorTextureView = this.colorTexture.createView();

        // Workers build render bundles
        for (let i = 0; i < kNumWorkers; i++) {
          this.workers[i].postMessage('frame');
        }

        const colorAttachment = {
            view: this.colorTextureView,
            clearValue: { r: 0, g: 0, b: 0, a: 1 },
            loadOp: 'clear',
            storeOp: 'store'
        };  
      
        const renderPassDesc = {
            colorAttachments: [colorAttachment],
        };

        const commandEncoder = this.device.createCommandEncoder();
      
        // 🖌️ Encode drawing commands
        const passEncoder = commandEncoder.beginRenderPass(renderPassDesc);
        passEncoder.setPipeline(this.pipeline);
        passEncoder.setViewport(
            0,
            0,
            this.canvas.width,
            this.canvas.height,
            0,
            1
        );

        if (this.numFinishedWorker >= kNumWorkers) {
          // console.log('submit render bundles');
          this.numFinishedWorker = 0;
          const bundles = new Array(kNumWorkers);
          for (let i = 0; i < kNumWorkers; i++) {
            bundles[i] = this.table.get(10 + i);
          }
          passEncoder.executeBundles(bundles);
        }

        passEncoder.end();

        this.device.queue.submit([commandEncoder.finish()]);

        // ➿ Refresh canvas
        requestAnimationFrame(this.render);
    };
}

// Main
const canvas = document.getElementById('gfx');
canvas.width = canvas.height = 640;
const renderer = new Renderer(canvas);
renderer.start();

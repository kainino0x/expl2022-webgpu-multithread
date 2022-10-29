const adapter = await navigator.gpu.requestAdapter();
const device = await adapter.requestDevice();

const table = new GPUSharedTable();
console.log('inserting into table', device);
table.insert(1, device);

const workerCount = parseInt(new URLSearchParams(window.location.search).get('workerCount')) || 1;
console.log(`Running with ${workerCount} workers.`);

const dispatchCount = 100000;
const dispatchCountPerWorker = dispatchCount / workerCount;

let workers = new Array(workerCount);
console.log('sending', table);
for (let i = 0; i < workerCount; ++i) {
  workers[i] = new Worker('./worker.js');
  workers[i].postMessage({ table });
}

const trialCount = 500;
let trialTimes = [];
let workerTimes = [];

function trial() {
  return new Promise(resolve => {
    let start = performance.now();
    let counts = 0;
    function onmessage(ev) {
      workerTimes.push(ev.data);
      counts += 1;
      if (counts % workerCount == 0) {
        trialTimes.push(performance.now() - start);
        resolve();
      }
    };
    for (let i = 0; i < workerCount; ++i) {
      workers[i].onmessage = onmessage;
      workers[i].postMessage({ dispatchCountPerWorker });
    }
  });
}

for (let i = 0; i < trialCount; ++i) {
  await trial();
}

trialTimes.sort();
workerTimes.sort();
console.log('median thread time', trialTimes[trialTimes.length / 2]);
console.log('median wall time', workerTimes[workerTimes.length / 2]);
console.log('average thread time', trialTimes.reduce((a, b) => a + b, 0) / trialTimes.length);
console.log('average wall time', workerTimes.reduce((a, b) => a + b, 0) / workerTimes.length);
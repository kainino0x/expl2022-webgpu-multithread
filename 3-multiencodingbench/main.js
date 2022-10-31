const adapter = await navigator.gpu.requestAdapter();
const device = await adapter.requestDevice();

const table = new GPUSharedTable();
console.log('inserting into table', device);
table.insert(1, device);

const workerCount = parseInt(new URLSearchParams(window.location.search).get('workerCount')) || 1;
console.log(`Running with up to ${workerCount} workers.`);

const dispatchCount = 100000;

let workers = new Array(workerCount);
console.log('sending', table);
for (let i = 0; i < workerCount; ++i) {
  workers[i] = new Worker('./worker.js');
  workers[i].postMessage({ table });
}

const trialCount = 500;
let trialTimes = [];
let encodingTimes = [];
let workerTimes = [];

function trial(numWorkers) {
  const dispatchCountPerWorker = dispatchCount / numWorkers;

  return new Promise(resolve => {
    let start = performance.now();
    let counts = 0;
    function onmessage(ev) {
      workerTimes.push(ev.data);
      counts += 1;
      if (counts % numWorkers == 0) {
        encodingTimes.push(performance.now() - start);

        // force a GPU flush
        device.queue.submit([]);
        device.queue.onSubmittedWorkDone().then(() => {
          trialTimes.push(performance.now() - start);
          resolve();
        });
      }
    };
    for (let i = 0; i < numWorkers; ++i) {
      workers[i].onmessage = onmessage;
      workers[i].postMessage({ dispatchCountPerWorker });
    }
  });
}

console.log('warming up...');
await trial(1);
await trial(workerCount);

console.log('starting');

const titles = [
  'workers',
  'median per-thread encoding time', 'median encoding time', 'median wall time',
  'average per-thread encoding time', 'average encoding time', 'average wall time'
];
let output = '';
output += titles.join('\t');
console.log(output);

const allResults = [];
// Test all worker counts from 1->workerCount
for (let numWorkers = 1; numWorkers <= workerCount; ++numWorkers) {
  trialTimes = [];
  encodingTimes = [];
  workerTimes = [];

  for (let i = 0; i < trialCount; ++i) {
    await trial(numWorkers);
  }

  workerTimes.sort();
  encodingTimes.sort();
  trialTimes.sort();
  const values = [
    numWorkers,
    workerTimes[workerTimes.length / 2],
    encodingTimes[encodingTimes.length / 2],
    trialTimes[trialTimes.length / 2],
    workerTimes.reduce((a, b) => a + b, 0) / workerTimes.length,
    encodingTimes.reduce((a, b) => a + b, 0) / encodingTimes.length,
    trialTimes.reduce((a, b) => a + b, 0) / trialTimes.length,
  ];
  let line = values.join('\t');
  console.log(line);
  output += '\n' + line;
}

console.log('\n\n==============================\n\n' + output);


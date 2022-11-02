const http = require('http');
const finalhandler = require('finalhandler');
const serveStatic = require('serve-static');

const serve = serveStatic("./");

const server = http.createServer(function(req, res) {
  res.setHeader('Cross-Origin-Embedder-Policy', 'require-corp');
  res.setHeader('Cross-Origin-Opener-Policy', 'same-origin');
  serve(req, res, finalhandler(req, res));
});

server.listen({
  host: '127.0.0.1',
  port: 8070,
});

// Try loading router step by step
try {
    const isPromise = require('is-promise');
    console.log('is-promise:', typeof isPromise);
    const Layer = require('./node_modules/router/lib/layer');
    console.log('Layer:', typeof Layer);
    const { METHODS } = require('http');
    console.log('METHODS:', METHODS);
    const parseUrl = require('parseurl');
    console.log('parseUrl:', typeof parseUrl);
    const Route = require('./node_modules/router/lib/route');
    console.log('Route:', typeof Route);
    const debug = require('debug')('router');
    console.log('debug:', typeof debug);
    console.log('All deps OK!');
    const router = require('router');
    console.log('router:', typeof router);
} catch(e) {
    console.log('Error:', e.message);
    console.log(e.stack.split('\n').slice(0,10).join('\n'));
}

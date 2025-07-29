// src/pkjs/index.js
var Clay = require("pebble-clay");
var clayConfig = require("./config");
var clay = new Clay(clayConfig);

clay.on("webviewclosed", function (e) {
  console.log("Page de config fermée");
});

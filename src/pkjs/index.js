const Clay = require("pebble-clay");
const clayConfig = require("./config");
const clay = new Clay(clayConfig);

clay.on("webviewclosed", function (e) {
  console.log("Config page closed");
});

/* *****************************************************************************
Copyright: Boaz Segev, 2020
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */

/* *****************************************************************************
This file defines a persistent WebSocket JSON Client interface.

This client only accepts JSON formatted messages.

Use the `emit` function to format data as JSON before sending it.

Use the `#on.event_name` to define event specific callbacks. Events are messages
that contain a JSON object with an `"event"` name.

Set `#on.default` to catch unknown events.

To route non-JSON messages (after a `try`-`catch` attempt) to `onmessage`, set
`REQUIRE_JSON_MESSAGES=false` before including this file.

example:

var c = new Client(); // or Client.new("wss://example.com/path");

c.on.ping = function(data) {
  console.log(data);
  data.event = 'pong';
  this.emit(data); // this == c
};
c.on.pong = function(data) { console.log(data); };

c.emit({event: 'ping' });
c.emit({event: 'fooBar' });

c.on.default = function(data) { console.warn("Unknown event!" ,data); };
// only valid when REQUIRE_JSON_MESSAGES == false was set:
c.onmessage = function(data) { console.warn("Unknown message:" ,data); };

***************************************************************************** */

if (typeof (REQUIRE_JSON_MESSAGES) == "undefined") {
  /** change this value to `false` to route non-JSON messages to `onmessage`. */
  REQUIRE_JSON_MESSAGES = true;
}

function Client(url) {
  // private data.
  this.private___ = {};
  this.private___.url = url;
  this.private___.connected = false;
  this.private___.interval = 0;
  if (!this.private___.url)
    this.private___.url = document.location.origin.replace(/^http/i, "ws");
  //
  this.on = {default : Client.default_message_handler};
  this.reconnect();
}

/** A default message handler. */
Client.default_message_handler = function(e) {
  if (typeof (e) == "string")
    console.warn(
        "Unknown message detected. Define the \'.onmessage\' function to handle this.",
        e);
  else if (e.event)
    console.warn("Unknown event \'" + e.event + "\'. Define the \'.on." +
                     e.event + "\' or \'.on.default\' function to handle it.",
                 e);
  else
    console.warn(
        "Incoming JSON message missing an \'event\' field. Define \'.on.default\' function to handle this.",
        e);
};

/** Reconnects to the WebSOcket server, IF the connection was lost. */
Client.prototype.reconnect = function() {
  if (this.ws && (this.ws.readyState == 0 || this.ws.readyState == 1))
    return;
  this.private___.autoconnect = true;
  this.ws = new WebSocket(this.private___.url);
  this.ws.owner = this;
  // The Websocket onopen callback
  this.ws.onopen = this.___on_open;
  // The Websocket onclose callback
  this.ws.onclose = this.___on_close;
  // The Websocket onerror callback
  this.ws.onerror = this.___on_error;
  // The Websocket onmessage callback
  this.ws.onmessage = this.___on_message;
};

/* routes the onopen callback after some housekeeping */
Client.prototype.___on_open = function(e) {
  this.owner.private___.connected = true;
  this.owner.private___.interval = 0;
  if (this.owner.onopen) {
    this.owner.onopen.call(this.owner, e);
  }
  if (!this.owner.onmessage)
    this.owner.onmessage = this.owner.on.default;
};

/* routes the onclose callback after some housekeeping */
Client.prototype.___on_close = function(e) {
  this.owner.private___.connected = false;
  if (this.owner.onclose) {
    this.owner.onclose.call(this.owner, e);
  }
  // increase reconnection attempt interval up to a 2 sec. interval
  this.owner.private___.interval =
      ((this.owner.private___.interval << 1) | 31) & 2047;
  if (this.owner.autoreconnect) {
    setTimeout(function(obj) { obj.reconnect(); },
               this.owner.private___.interval, this.owner);
  }
};

/* routes the onerror callback after some housekeeping */
Client.prototype.___on_error = function(e) {
  if (this.owner.onerror) {
    this.owner.onerror.call(this.owner, e);
  }
};

if (REQUIRE_JSON_MESSAGES) {
  /* routes the onmessage callback after some housekeeping */
  Client.prototype.___on_message = function(e) {
    var msg;
    try {
      msg = JSON.parse(e.data);
    } catch (err) {
      console.error(
          "Client experienced an error parsing the following data (not JSON):",
          e.data, err, e);
      return;
    }
    if (this.owner.on[msg["event"]])
      this.owner.on[msg["event"]].call(this.owner, msg);
    else
      this.owner.on.default.call(this.owner, msg);
  };
} else {
  /* routes the onmessage callback after some housekeeping */
  Client.prototype.___on_message = function(e) {
    var msg = false;
    try {
      msg = JSON.parse(e.data);
    } catch (err) {
    }
    if (!msg)
      this.owner.onmessage.call(this.owner, e.data);
    else if (this.owner.on[msg["event"]])
      this.owner.on[msg["event"]].call(this.owner, msg);
    else
      this.owner.on.default.call(this.owner, msg);
  };
}
/** Closes the connection */
Client.prototype.close = function() {
  this.private___.autoconnect = false;
  this.ws.close();
};

/** Formats the data in JSON format and sends it to the server. */
Client.prototype.emit = function(data) { this.send(JSON.stringify(data)); };

/** Sends raw data to the server. */
Client.prototype.send = function(data) {
  try {
    this.ws.send(data);
  } catch {
    this.reconnect();
    setTimeout(this.send.bind(this), 10, data);
  }
};

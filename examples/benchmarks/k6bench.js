import ws from "k6/ws"; 

export default function() { 

  let url = "ws://localhost:3000";  
  let params = { "tags": { "my_tag": "hello" } };   

  let res = ws.connect(url, params, function(socket) {  

      socket.on('open', function open() {   
          console.log('connected'); 

          socket.setInterval(function timeout() {   
            socket.ping();  
            console.log("Pinging every 1sec (setInterval test)");   
          }, 1000); 
      });   

      socket.on('ping', function () {   
          console.log("PING!"); 
      });   

  
      socket.on('pong', function () {   
          console.log("PONG!"); 
      });   

      socket.on('close', function() {   
          console.log('disconnected');  
      });   

      socket.setTimeout(function () {   
          console.log('2 seconds passed, closing the socket');  
          socket.close();   
      }, 2000); 
});
}

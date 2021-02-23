
#[cxx::bridge(namespace = "sp::rust")]
mod ffi {
    extern "Rust" {
        fn print_hello();
        fn http_get(url: String) -> String;
    }
}

use tokio::net::{TcpListener, TcpStream};
use tokio::io::{self, AsyncReadExt, AsyncWriteExt};

#[tokio::main]
pub async fn print_hello() {
    println!("hello world!");

    // Bind the listener to the address
    let listener = TcpListener::bind("127.0.0.1:8000").await.unwrap();

    println!("Listening");

    loop {
        // The second item contains the IP and port of the new connection.
        let (socket, _) = listener.accept().await.unwrap();
        
        println!("Accepted");
        tokio::spawn(async move {
            process(socket).await?;
            Ok::<_, io::Error>(())
        });
    }
}

pub fn http_get(url: String) -> String {
    let res = reqwest::blocking::get(url::Url::parse(&url).ok().unwrap());
    if res.is_ok() {
        let content = res.ok().unwrap().text();
        if content.is_ok() {
            return content.ok().unwrap();
        } else {
            return "no content".to_string();
        }
    } else {
        return "error".to_string();
    }
}

async fn process(socket: TcpStream) -> io::Result<()> {
    println!("Connected {:?}", socket.peer_addr().unwrap());

    let (mut rd, mut wr) = io::split(socket);

    // Write data in the background
    tokio::spawn(async move {
        wr.write_all(b"hello\r\n").await?;
        wr.write_all(b"world\r\n").await?;
        Ok::<_, io::Error>(())
    });

    let mut buf = vec![0; 128];

    loop {
        let n = rd.read(&mut buf).await?;

        if n == 0 {
            break;
        }

        println!("GOT {:?}", &buf[..n]);
    }

    println!("Disconnected");

    Ok(())
}

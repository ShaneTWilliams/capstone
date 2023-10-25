// Prevents additional console window on Windows in release, DO NOT REMOVE!!
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use std::thread;

use ground::ground_station_client::GroundStationClient;
use ground::ValueRequest;

pub mod ground {
    tonic::include_proto!("capstone");
}

#[tauri::command]
async fn my_custom_command() -> serde_json::Value {
    let mut client = GroundStationClient::connect("http://localhost:50051").await.unwrap();

    let request = tonic::Request::new(ValueRequest {
        tag: 0,
    });

    let response = client.get_value(request).await.unwrap();

    println!("RESPONSE={:?}", response);
    serde_json::json!({
        "value": "bar"
    })
}

fn main() {
    tauri::Builder::default()
        .invoke_handler(tauri::generate_handler![my_custom_command])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}

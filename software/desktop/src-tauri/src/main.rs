// Prevents additional console window on Windows in release, DO NOT REMOVE!!
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use ground::ground_station_client::GroundStationClient;
use ground::Value;
use ground::ValueRequest;
use ground::value::Value::{B, U32, I32, U64, I64, F32, F64};
use tonic::Response;
use tokio;

pub mod ground {
    tonic::include_proto!("capstone");
}

struct AppState {
    client: GroundStationClient<tonic::transport::Channel>
}

#[tauri::command]
#[tokio::main]
async fn get_value(mut state: tauri::State<AppState>, tag: i32) -> serde_json::Value {
    let request = tonic::Request::new(ValueRequest {
        tag: tag,
    });
    println!("{:?}", request);
    match state.client.get_value(request).await {
        Ok(response) => match response.into_inner() {
            Value{value: Some(value)} => match value {
                B(b) => serde_json::Value::Bool(b),
                U32(u32_value) => serde_json::json!({"value": u32_value}),
                I32(i32_value) => serde_json::json!({"value": i32_value}),
                U64(u64_value) => serde_json::json!({"value": u64_value}),
                I64(i64_value) => serde_json::json!({"value": i64_value}),
                F32(f32_value) => serde_json::json!({"value": f32_value}),
                F64(f64_value) => serde_json::json!({"value": f64_value}),
            },
            _ => panic!()
        },
        _ => panic!()
    }
}

#[tokio::main]
async fn main() {
    let mut client = GroundStationClient::connect("http://localhost:50051").await.unwrap();

    tauri::Builder::default()
        .invoke_handler(tauri::generate_handler![get_value])
        .manage(AppState{client})
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}

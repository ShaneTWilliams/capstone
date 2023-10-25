import "./App.css";

// When using the Tauri API npm package:
import { invoke } from '@tauri-apps/api/tauri'

import { useEffect, useState } from "react";

function App() {
  const [time, setTime] = useState(Date.now());
  const [val, setVal] = useState("a");

  useEffect(() => {
    invoke("my_custom_command").then((message) => setVal(message));
    const interval = setInterval(() => setTime(Date.now()), 100);
    return () => {
      clearInterval(interval);
    }
  }, []);
  return (
    <div className="container">
      <p className="text-red-700">test  {val["value"]}</p>
    </div>
  );
}

export default App;

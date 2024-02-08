import "./App.css";

// When using the Tauri API npm package:
import { invoke } from '@tauri-apps/api/tauri'

import { useEffect, useState } from "react";

function App() {
  const [time, setTime] = useState(Date.now());
  const [val, setVal] = useState({});

  useEffect(() => {
    const interval = setInterval(() => {
      invoke("get_value", {tag: 1}).then((message) => setVal(message["value"]));
      setTime(Date.now())
    }, 100);
    return () => {
      clearInterval(interval);
    }
  }, []);

  return (
    <div className="container">
      <p className="text-zinc-200">Test {time} {val}</p>
    </div>
  );
}

export default App;

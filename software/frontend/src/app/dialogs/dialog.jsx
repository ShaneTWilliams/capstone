export default function ({ onExit, children }) {
  return (
        <div className="fixed flex flex-row justify-center items-center h-screen w-screen ">
            <div
                onClick={() => onExit(false)}
                className="flex flex-row justify-center items-center h-screen w-screen fixed bg-black opacity-60 dark:opacity-80"
            ></div>
            <div className="flex flex-none flex-col justify-start items-start bg-neutral-50 w-3/4 h-3/4 rounded-lg z-0 min-h-[350px] min-w-[550px] p-8 dark:bg-zinc-800">
                {children}
            </div>
        </div>
  )
}

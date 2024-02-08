import Dialog from './dialog'

export default function ({ onExit }) {
    return (
        <Dialog onExit={onExit}>
            <div className="flex flex-col grow w-full h-full">
                <p className="flex-none text-2xl font-medium text-black mb-4 dark:text-zinc-300">
                    Status
                </p>
                <div className="flex grow flex-row w-full overflow-auto">
                    test
                </div>
            </div>
        </Dialog>
    )
}

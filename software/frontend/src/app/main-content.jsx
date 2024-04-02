import { useState, useEffect } from 'react'

function Configuration({ name, bottomBorder, handleClick }) {
    return (
        <div key={name} className="group" onClick={() => handleClick(name)}>
            <div
                className="flex flex-none flex-row py-2 px-3 items-center active:bg-neutral-200 bg-neutral-100 group-first:rounded-t-md group-last:rounded-b-md dark:bg-zinc-800 dark:active:bg-zinc-700"
                key={name}
            >
                <p className="flex grow text-sm w-32 dark:text-zinc-300">
                    {name}
                </p>
            </div>
            {bottomBorder && (
                <div className="border-b mx-2 border-neutral-200 dark:border-zinc-700"></div>
            )}
        </div>
    )
}

export default function ({ content, title }) {
    return (
        <div className="w-full flex flex-col h-full">
            <div className="flex flex-none flex-row justify-left items-end border-neutral-200 border-b h-20 p-3 dark:border-zinc-700">
                <p className="flex text-2xl font-medium ml-2 text-neutral-800 dark:text-zinc-50 mr-4">
                    {title}
                </p>
                <p className="flex text-lg ml-2 dark:text-zinc-500 text-neutral-400" data-tooltip-id="serial-tooltip" data-tooltip-place="bottom">
                    Last Heartbeat: 44 ms
                </p>
            </div>
            <div className="p-4 overflow-scroll">
                {content}
            </div>
        </div>
    )
}

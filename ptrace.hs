module Main where

import Data.Word
import System.Posix.Process ( forkProcess, getProcessStatus )
import System.Exit
import System.Cmd (rawSystem)

import Foreign.Ptr

import System.Linux.Ptrace
import System.Linux.Ptrace.Syscall
import System.Linux.Ptrace.Types

-- | A command to run, with optional arguments.
command :: (FilePath, [String])
command = ("/bin/ls", [])

-- orig_eax_ptr = nullPtr `plusPtr` 11

orig_eax_ptr :: RemotePtr Word
orig_eax_ptr = fromInteger 11 :: RemotePtr Word

main :: IO ()
main = do
  putStrLn $ "orig_eax ptr address: "++show orig_eax_ptr
  childProc <- traceProcess =<< (forkProcess $ child command)
  continue childProc
  oeVal <- ptrace_peekuser (pid childProc) orig_eax_ptr
  putStrLn $ "The child made syscall "++(show oeVal)
  _ <- getProcessStatus True True (pid childProc)
  exitSuccess

child :: (FilePath, [String]) -> IO ()
child cmd = do
  uncurry rawSystem cmd
  ptrace_traceme
  return ()

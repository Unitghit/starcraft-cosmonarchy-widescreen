Copy the buildscripts and rename them to non _ versions. 

As an example my Debug file looks like this:

:: Batch code from this file will run after building a Debug version of Aize
:: Please don't commit your version of this file to the repository
copy "Debug\aize_debug.qdp" "D:\Cosmonarchy\Release\plugins\aize_debug.qdp"
copy "Debug\aize_debug.qdp" "D:\Cosmonarchy\Prerelease\plugins\aize_debug.qdp"
copy "Debug\aize_debug.qdp" "D:\Cosmonarchy\Dev\plugins\aize_debug.qdp"

The other files would use FastDebug\ and Release\ in the left most paths
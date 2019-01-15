# FileSignature
The project provides a way of a quick transformation of the content of some file. 
There is a file on the file system. It's need to make transformation of the file's content and save result to another file. The main aspect of realization is time of transformation, performance. It's implemented a conveyer file transformation model. Idea of the project is somehow similar with ideas of streaming. A one thread reads content from a file to queue, one - transform content and another writes results in a file. 
The code is crossplatform, but I've push just a VS project.
There are External dependences in project settings 
- on boost(boost::program_options). The project is linked with a locally builded directory of boost. Please change links in your system in a right way
There is an external lib easylogging++ for logging. I've commit code of the lib to the project. So it builds inside project. See file logger.config for better configuration of logger. Defaultly logs are being  writing to the app.log file.
Please, change default settings of program options for debugging if it needs for You.
Good luck
I.

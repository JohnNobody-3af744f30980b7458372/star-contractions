args <- commandArgs(trailingOnly=TRUE)

cols <- c("mst", "mstp", "zel", "zelm", "zelp")
extra_cols <- c("", "starW", "starS")

N <- 100

read_file <- function (filename) {
  col_names <- append(append(append(
    c( "round", "", "V", "E", "T", "",  "W1", "W2", ""), cols), extra_cols), c(""))
  txt <- readLines(filename)
  return(read.table(textConnection(txt[grepl("@@", txt)]), sep = ",",
                  col.names = col_names, header = FALSE))
}

get_100 <- function (filename) {
  min_w <- NULL
  bare_name <- sub("\\.(no)?prec\\.(no)?zel\\.log$", "", filename)
  for (zel in c("no", "")) for (prec in c("no", "")) {
    f <- sprintf("%s.%sprec.%szel.log", bare_name, prec, zel)
    tryCatch({
      closeAllConnections()
      d <- read_file(f)
      cols <- c("mst", "mstp")
      if (zel == "") cols <- append(cols, c( "zel", "zelm", "zelp"))
      for (c in cols) min_w <- min(min_w, min(d[[c]]))

      txt <- readLines(f)
      fin <- strtoi(sub("## FINAL WEIGHT ", "", txt[grepl("FINAL", txt)]))
      min_w <- min(min_w, fin)
    }, warning = function (w) {}, error = function (e) {})
  }
  
  return(min_w)
}

read_and_normalize <- function (filename) {
  d <- read_file(filename)

  t_max <- max(d$T)
  t_min <- min(d$T)
  d["t_norm"] <- (t_max - d$T) / (t_max - t_min)
  n <- 1.0 * d$V[1]
  t <- 1.0 * d$T[1]

  min_w <- get_100(filename)

  for (c in cols) d[c] <- 100 * d[c] / min_w

  d_res <- data.frame(0:N)
  names(d_res) <- c("x")
  for (c in cols)
    d_res[c] <- approx(d$t_norm, d[[c]], n = N+1, method = "linear")$y

  d["W1c"] <- cumsum(as.numeric(d$W1)) / (n*t)
  d["W2c"] <- cumsum(as.numeric(d$W2)) / (n*n*t)

  d_res["W1c"] <- approx(d$t_norm, d$W1c, n = N+1, method = "linear")$y
  d_res["W2c"] <- approx(d$t_norm, d$W2c, n = N+1, method = "linear")$y

  d_res["W1"] <- approx(d$t_norm, d$W1, n = N+1, method = "linear")$y / n
  d_res["W2"] <- approx(d$t_norm, d$W2, n = N+1, method = "linear")$y / (n*n)

  return(d_res)
}

i <- 0
D <- data.frame()

for (f in 3:length(args)) {
  tryCatch({
    d <- read_and_normalize(args[f])
    d["file"] <- args[f]
    i <- i + 1
    D <- rbind(D, d)
  }, error = function(e) {
    write(paste("Error in ", args[f]), stderr())
  })
}

message(i)

agg <- data.frame(0:N)
names(agg) <- c("x")

cols <- append(cols, c("W1", "W2"))

switch(args[[2]],
  avg = { for (c in cols) agg[c] <- cbind(by(D[,c], D$x, mean)) },
  median = { for (c in cols) agg[c] <- cbind(by(D[,c], D$x, median)) },
  min = { for (c in cols) agg[c] <- cbind(by(D[,c], D$x, min)) },
  max = { for (c in cols) agg[c] <- cbind(by(D[,c], D$x, max)) },
  polak = {
    for (c in cols) agg[c] <- cbind(by(D[,c], D$x, mean))
    ret <- data.frame(c(0))
    ret["mst"] <- agg[41, "mst"]
    ret["mstp"] <- agg[41, "mstp"]
    ret["zel"] <- agg[1, "zel"]
    ret["zelm"] <- agg[1, "zelm"]
    ret["zelp"] <- agg[1, "zelp"]
    agg <- ret
  },
  mst_box = {
    agg["min"] <- cbind(by(D[,args[[1]]], D$x, min))
    agg["q1"] <- cbind(by(D[,args[[1]]], D$x, function (x) quantile(x, 0.25)))
    agg["median"] <- cbind(by(D[,args[[1]]], D$x, median))
    agg["q3"] <- cbind(by(D[,args[[1]]], D$x, function (x) quantile(x, 0.75)))
    agg["max"] <- cbind(by(D[,args[[1]]], D$x, max))

    for (c in c("min", "q1", "median", "q3", "max"))
      agg[c] <- sapply(agg[c], format, digits = 2, nsmall=2)

    agg <- agg[which(agg$x %% (N / 10) == 0), ] 
  },
  avg2 = {
    agg["avg"] <- cbind(by(D[,args[[1]]], D$x, mean))
    agg["avg"] <- sapply(agg["avg"], format, digits = 2, nsmall=2)
    # agg <- agg[which(agg$x %% (N / 100) == 0), ] 
  },
  mst_box2 = {
    agg <- data.frame(1:i)
    names(agg) <- c("filenr")
    for (i in 0:10) {
      agg[paste("x", i, sep="")] <- D[which(D$x == 10*i), args[[1]]]
    }
  }
)

write.table(agg, row.names=FALSE, col.names=FALSE, sep=",")


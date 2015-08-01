% This function loads an image (or matrix), and plots a histogram from it
function h = img2hist(fileName, bins, name)

if ischar(fileName)
    fileName = strvcat2cell(fileName);
end
if (nargin >= 3) && ischar(name)
    name = strvcat2cell(name);
end

%% tSNR results figure!
h = figure;
hold on

colorsB = distinguishable_colors(length(fileName));
histVals = cell(size(fileName));
Y = cell(size(fileName));

maxV = 0;
legStr = cell(1,numel(fileName));

for f = 1:numel(fileName)
    
    % Get image or matrix...
    if ischar(fileName{f})
        Y{f} = spm_read_vols(spm_vol(fileName{f}));
    else
        Y{f} = fileName{f};
    end
    
    % Linearise (exclude NaN and zero values)
    Y{f} = Y{f}(and(~isnan(Y{f}), Y{f} ~= 0));
    
    % Parameters for histogram
    if nargin < 2 || isempty(bins)
        aMax = ceil(max(abs(Y{f})));
        bins =  -aMax:(aMax/100):aMax;
    end
    if nargin < 3
        name{f} = 'Image';
    end
    
    % Now make a histogram and "normalise" it
    histVals{f} = hist(Y{f}, bins);
    histVals{f} = histVals{f}./sum(histVals{f});
    % And decide what is the greatest prop value
    maxV = max(max(histVals{f}), maxV);
    
    % Make bars semi-transparent for overlaying
    B = bar(bins, histVals{f}, 1, 'FaceColor', colorsB(f,:));
    ch = get(B,'child');
    set(ch, 'faceA', 0.3, 'edgeA', 0.2);
    
    % T-value of deviation from zero-centered
    zmean = Y{f}-mean(Y{f});
    Tstats = testt(Y{f},zmean);
    SRstats = wilcoxon(Y{f}',zmean');
    
    % Get legend information
    legStr{f} = sprintf('%s: mean %0.2f, median %0.2f, std: %0.2f, ttest-Tval is: %0.2f, SR-Zval is: %0.2f', ...
        name{f}, nanmean(Y{f}), nanmedian(Y{f}), nanstd(Y{f}), Tstats.tvalue, SRstats.z);
end

xlabel('Value')
ylabel('Proportion of voxels')
legend(legStr);

%% Difference between 2 distributions
if length(fileName) == 2
    try
        Tstats = testt(Y{1},Y{2});
        SRstats = wilcoxon(Y{1},Y{2});
        title(sprintf('%s: mean %0.2f, median %0.2f, ttest-Tval is: %0.2f, SR-Zval is: %0.2f', ...
            [name{1} '-' name{2}], nanmean(Y{1}) - nanmean(Y{2}), nanmedian(Y{1}) - nanmedian(Y{2}), Tstats.tstat, SRstats.zval))
    catch
        [nh, p, dev, Tstats] = ttest2(Y{1},Y{2});
        title(sprintf('%s: mean %0.2f, median %0.2f, ttest-Tval is: %0.2f', ...
            [name{1} '-' name{2}], nanmean(Y{1}) - nanmean(Y{2}), nanmedian(Y{1}) - nanmedian(Y{2}), Tstats.tstat))
    end
end
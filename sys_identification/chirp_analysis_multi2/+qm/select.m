function [trials,S,metadata,referenceCheck] = select(o,out)
files=dir(fullfile(o.captureDir,'chirp_*.csv'));
[~,order]=sort({files.name}); files=files(order);
refs=["chirp_20261020_101838_880284.csv","chirp_20261020_102301_899188.csv"];
% Metadata is always read, including old and incompatible files.
N=numel(files); metadata=cell(N,1); data=cell(N,1); errors=strings(N,1);
for i=1:N
    try
        [data{i},metadata{i}]=qm.readCapture(fullfile(files(i).folder,files(i).name));
    catch e
        errors(i)=string(e.message);
        % Preserve every readable comment even if the numeric table is malformed.
        lines=splitlines(string(fileread(fullfile(files(i).folder,files(i).name))));
        tok=regexp(cellstr(lines),'^\s*#\s*(.*?)\s*=\s*(.*)$','tokens','once');
        tok=tok(~cellfun(@isempty,tok));
        if ~isempty(tok)
            vals=vertcat(tok{:}); metadata{i}=cell2table(vals,'VariableNames',{'Key','Value'});
            metadata{i}.Key=string(metadata{i}.Key); metadata{i}.Value=string(metadata{i}.Value);
        end
    end
end
idx=find(string({files.name})==refs(2));
assert(isscalar(idx),'Required reference CSV missing');
R=metadata{idx};
idx1=find(string({files.name})==refs(1));
assert(isscalar(idx1),'First reference CSV missing');
referenceCheck=qm.compareMetadata(metadata{idx1},R);
% Pin requested protocol values; do not silently accept a changed reference.
c=qm.conditions(R);
assert(max(abs([c.f0 c.f1 c.T c.A0 c.A1 c.a c.fs c.limit c.preDuration c.postDuration]- ...
    [1 100 20 1.5 10 1 2000 15 2 1]))<1e-6,'Reference differs from requested experiment');
assert(abs(c.Kp*pi/180-.00834)<1e-8 && abs(c.Kd-.1)<1e-6 && ...
    qm.number(R,'CONFIG.MotorID')==2 && qm.number(R,'CONFIG.CAN_bitrate')==1e6, ...
    'Reference PD/motor settings differ from requested experiment');
trials={}; selected=false(N,1); dates=NaT(N,1); reason=strings(N,1);
q0=nan(N,1); valid=strings(N,1); complete=valid; status=valid;
allmeta=table;
for i=1:N
    name=string(files(i).name); reasons=strings(0,1); M=metadata{i};
    if ~isempty(M)
        part=addvars(M,repmat(name,height(M),1),'Before',1,'NewVariableNames','file');
        allmeta=[allmeta;part]; %#ok<AGROW>
        q0(i)=str2double(get(M,"REFERENCE_POSITION_DEG (theta_ref)"));
        valid(i)=get(M,"RESULT.valid"); complete(i)=get(M,"RESULT.complete"); status(i)=get(M,"RESULT.status_name");
        reasons=[reasons;qm.compareMetadata(M,R)];
    end
    token=regexp(name,'^chirp_(\d{8}_\d{6}_\d{6})\.csv$','tokens','once');
    if isempty(token)
        reasons(end+1)="ambiguous filename timestamp";
    else
        dates(i)=datetime(token{1},'InputFormat','yyyyMMdd_HHmmss_SSSSSS');
        cutoff=datetime('20261020_102301_899188','InputFormat','yyyyMMdd_HHmmss_SSSSSS');
        if ~ismember(name,refs) && dates(i)<=cutoff, reasons(end+1)="older than cutoff"; end
    end
    if errors(i)~="", reasons(end+1)="read: "+errors(i);
    else
        try
            [d,cc,ck]=qm.validate(data{i},M,o);
        catch e
            reasons(end+1)="health: "+string(e.message);
        end
    end
    if ~isempty(o.files) && ~ismember(name,string(o.files)), reasons(end+1)="not in requested subset"; end
    if ~isempty(referenceCheck), reasons(end+1)="reference pair disagrees"; end
    selected(i)=isempty(reasons); reason(i)=strjoin(reasons,"; ");
    if selected(i)
        trials{end+1}=struct('file',name,'c',cc,'d',d,'checks',ck); %#ok<AGROW>
    end
end
dates.Format='yyyy-MM-dd HH:mm:ss.SSSSSS';
S=table(string({files.name})',dates,q0,valid,complete,status,selected,reason, ...
    'VariableNames',{'file','recordedAt','reference_deg','valid','complete','status','selected','reason'});
% Wide selection table contains every metadata field, with original key mapping.
keys=unique(allmeta.Key,'stable');
varNames=matlab.lang.makeUniqueStrings(matlab.lang.makeValidName(cellstr("meta_"+keys)));
for k=1:numel(keys)
    col=strings(N,1);
    for i=1:N, col(i)=get(metadata{i},keys(k)); end
    S.(varNames{k})=col;
end
writetable(S,fullfile(out,'file_selection.csv'));
writetable(allmeta,fullfile(out,'all_metadata.csv'));
writetable(table(keys,string(varNames(:)),'VariableNames',{'metadata_key','selection_column'}),fullfile(out,'metadata_columns.csv'));
fprintf('Selected %d / %d CSVs; excluded %d.\n',sum(selected),N,sum(~selected));
assert(~isempty(trials),'No eligible trials; inspect file_selection.csv');
end
function v=get(M,key)
v=""; if isempty(M), return; end
x=M.Value(M.Key==key); if isscalar(x), v=x; end
end
